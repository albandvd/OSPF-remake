#!/bin/sh
# Automated demo of the OSPF-remake lab: wait for convergence, show the
# routing tables, exercise the ping matrix, simulate a router failure,
# show the failover, simulate recovery, and print a final PASS/FAIL
# summary with measured convergence times.
#
# Runs as the "scenario" service in docker-compose.yml, talking to the
# other containers through the Docker socket mounted read-only into this
# container (sibling-container pattern — this container never runs the
# daemon itself, it only calls `docker exec/stop/start` on the others).

set -u

ROUTERS="r1 r2 r3 r4 r5"
TERMINALS="t1 t2 t3"
ALL_NETS="10.1.0.0/24 10.2.0.0/24 10.3.0.0/24 192.168.1.0/24 192.168.2.0/24 192.168.3.0/24"
CONVERGENCE_TIMEOUT=120
RESULTS_FILE=/tmp/scenario_results.txt
: > "$RESULTS_FILE"

RED='\033[31m'; GREEN='\033[32m'; YELLOW='\033[33m'; BLUE='\033[34m'; BOLD='\033[1m'; RESET='\033[0m'

banner() {
    printf '\n%s%s=== %s ===%s\n\n' "$BOLD" "$YELLOW" "$1" "$RESET"
}

log_result() {
    # $1=PASS|FAIL  $2=description
    printf '%s|%s\n' "$1" "$2" >> "$RESULTS_FILE"
    if [ "$1" = PASS ]; then
        printf '%s[PASS]%s %s\n' "$GREEN" "$RESET" "$2"
    else
        printf '%s[FAIL]%s %s\n' "$RED" "$RESET" "$2"
    fi
}

# Non-empty output from `ip route show <net>` means the router currently
# has a route for that exact network (connected or learned).
router_has_route() {
    router="$1"; net="$2"
    out=$(docker exec "$router" ip route show "$net" 2>/dev/null)
    [ -n "$out" ]
}

router_running() {
    [ "$(docker inspect -f '{{.State.Running}}' "$1" 2>/dev/null)" = "true" ]
}

# Wait until every router in $1 (space-separated) has a route to every
# network in $2. Prints the elapsed seconds to stdout on success or
# timeout; return status reflects success.
wait_for_routes() {
    routers="$1"; nets="$2"
    start=$(date +%s)
    while :; do
        ok=1
        for r in $routers; do
            if ! router_running "$r"; then ok=0; continue; fi
            for n in $nets; do
                if ! router_has_route "$r" "$n"; then
                    ok=0
                fi
            done
        done
        now=$(date +%s)
        elapsed=$((now - start))
        if [ "$ok" = 1 ]; then
            echo "$elapsed"
            return 0
        fi
        if [ "$elapsed" -ge "$CONVERGENCE_TIMEOUT" ]; then
            echo "$elapsed"
            return 1
        fi
        sleep 2
    done
}

# Wait until none of the routers in $1 has a route whose next-hop is in
# $2 (space-separated IPs) — used to confirm a crashed router's routes
# have been purged everywhere.
wait_no_route_via() {
    routers="$1"; bad_hops="$2"
    start=$(date +%s)
    while :; do
        ok=1
        for r in $routers; do
            if ! router_running "$r"; then continue; fi
            table=$(docker exec "$r" ip route 2>/dev/null)
            for hop in $bad_hops; do
                if echo "$table" | grep -q "$hop"; then
                    ok=0
                fi
            done
        done
        now=$(date +%s)
        elapsed=$((now - start))
        if [ "$ok" = 1 ]; then
            echo "$elapsed"
            return 0
        fi
        if [ "$elapsed" -ge "$CONVERGENCE_TIMEOUT" ]; then
            echo "$elapsed"
            return 1
        fi
        sleep 2
    done
}

show_tables() {
    routers="$1"
    for r in $routers; do
        if ! router_running "$r"; then
            printf '%s-- %s --%s %s(not running)%s\n\n' "$BLUE" "$r" "$RESET" "$RED" "$RESET"
            continue
        fi
        printf '%s-- %s: effective routing table --%s\n' "$BLUE" "$r" "$RESET"
        docker exec "$r" /opt/ospf/show routes 2>&1
        printf '\n%s-- %s: kernel ip route --%s\n' "$BLUE" "$r" "$RESET"
        docker exec "$r" ip route 2>&1
        echo
    done
}

# $1=from  $2=to-ip  $3=description  $4=expect("pass"|"fail")
run_ping() {
    from="$1"; to="$2"; desc="$3"; expect="$4"
    printf '%s$ docker exec %s ping -c 3 -W 1 %s%s\n' "$BOLD" "$from" "$to" "$RESET"
    if docker exec "$from" ping -c 3 -W 1 "$to"; then
        got=pass
    else
        got=fail
    fi
    if [ "$got" = "$expect" ]; then
        log_result PASS "$desc"
    else
        log_result FAIL "$desc (expected $expect, observed $got)"
    fi
    echo
}

run_traceroute() {
    from="$1"; to="$2"; desc="$3"
    printf '%s$ docker exec %s traceroute -n -w 1 -q 1 %s%s\n' "$BOLD" "$from" "$to" "$RESET"
    docker exec "$from" traceroute -n -w 1 -q 1 "$to" 2>&1
    echo
    log_result PASS "$desc (informational)"
}

# ---------------------------------------------------------------------------

banner "OSPF-REMAKE DEMO SCENARIO"
echo "Waiting for the lab containers to be reachable..."
for r in $ROUTERS $TERMINALS; do
    tries=0
    until docker inspect "$r" >/dev/null 2>&1; do
        tries=$((tries + 1))
        if [ "$tries" -gt 60 ]; then
            echo "Container $r never appeared, aborting."
            exit 1
        fi
        sleep 1
    done
done

banner "STEP 1/7 — INITIAL CONVERGENCE"
T_INIT=$(wait_for_routes "$ROUTERS" "$ALL_NETS")
if [ "$T_INIT" -lt "$CONVERGENCE_TIMEOUT" ]; then
    log_result PASS "initial convergence (all 5 routers know all 6 networks) in ${T_INIT}s"
else
    log_result FAIL "initial convergence did not complete within ${CONVERGENCE_TIMEOUT}s"
fi
echo "Convergence time: ${T_INIT}s"

banner "STEP 2/7 — ROUTING TABLES (converged)"
show_tables "$ROUTERS"

banner "STEP 3/7 — PING MATRIX (converged)"
run_ping t1 192.168.2.10 "t1 -> t2 reachable" pass
run_ping t1 192.168.3.10 "t1 -> t3 reachable" pass
run_ping t2 192.168.3.10 "t2 -> t3 reachable" pass
run_traceroute t1 192.168.3.10 "t1 -> t3 path"

banner "STEP 4/7 — SIMULATING FAILURE OF r2"
printf '%s%s>>> docker stop r2 <<<%s\n\n' "$BOLD" "$RED" "$RESET"
echo "r2 sits on two equal-cost paths used by t1 <-> t3 (via r2 or via r4), and"
echo "t2 is single-homed behind it — so after the crash, t1 <-> t3 must fail over"
echo "to r4, while t2 is expected to become unreachable."
docker stop r2 >/dev/null

banner "STEP 5/7 — RECONVERGENCE AFTER FAILURE"
SURVIVORS="r1 r3 r4 r5"
NETS_WITHOUT_T2="10.1.0.0/24 10.2.0.0/24 10.3.0.0/24 192.168.1.0/24 192.168.3.0/24"
T_FAIL=$(wait_no_route_via "$SURVIVORS" "10.1.0.2 10.2.0.2")
T_FAIL2=$(wait_for_routes "$SURVIVORS" "$NETS_WITHOUT_T2")
[ "$T_FAIL2" -gt "$T_FAIL" ] && T_FAIL=$T_FAIL2
if router_running r2; then
    log_result FAIL "r2 failure reconvergence"
else
    log_result PASS "reconverged around r2's failure in ${T_FAIL}s (no stale route via r2, all surviving networks still reachable)"
fi
echo "Reconvergence time after failure: ${T_FAIL}s"

show_tables "$SURVIVORS"

run_traceroute t1 192.168.3.10 "t1 -> t3 path now avoids r2"
run_ping t1 192.168.3.10 "t1 -> t3 still reachable (failover via r4)" pass
echo "The next check is an EXPECTED failure: t2 is behind the crashed r2."
run_ping t1 192.168.2.10 "t1 -> t2 unreachable (EXPECTED — r2 is down)" fail

banner "STEP 6/7 — RECOVERY OF r2"
printf '%s%s>>> docker start r2 <<<%s\n\n' "$BOLD" "$GREEN" "$RESET"
docker start r2 >/dev/null
T_RECOVER=$(wait_for_routes "$ROUTERS" "$ALL_NETS")
if [ "$T_RECOVER" -lt "$CONVERGENCE_TIMEOUT" ]; then
    log_result PASS "reconverged after r2's recovery in ${T_RECOVER}s"
else
    log_result FAIL "did not reconverge after r2's recovery within ${CONVERGENCE_TIMEOUT}s"
fi
echo "Reconvergence time after recovery: ${T_RECOVER}s"

show_tables "$ROUTERS"

banner "STEP 7/7 — PING MATRIX (recovered)"
run_ping t1 192.168.2.10 "t1 -> t2 reachable again after recovery" pass
run_ping t1 192.168.3.10 "t1 -> t3 reachable after recovery" pass
run_ping t2 192.168.3.10 "t2 -> t3 reachable after recovery" pass

banner "SUMMARY"
pass=0
fail=0
while IFS='|' read -r status desc; do
    if [ "$status" = PASS ]; then
        printf '  %s[PASS]%s %s\n' "$GREEN" "$RESET" "$desc"
        pass=$((pass + 1))
    else
        printf '  %s[FAIL]%s %s\n' "$RED" "$RESET" "$desc"
        fail=$((fail + 1))
    fi
done < "$RESULTS_FILE"

echo
echo "Convergence times:"
echo "  initial convergence       : ${T_INIT}s"
echo "  reconvergence after crash : ${T_FAIL}s"
echo "  reconvergence after start : ${T_RECOVER}s"
echo

total=$((pass + fail))
if [ "$fail" -eq 0 ]; then
    printf '%s%sALL CHECKS PASSED (%d/%d)%s\n' "$BOLD" "$GREEN" "$pass" "$total" "$RESET"
    exit 0
else
    printf '%s%s%d CHECK(S) FAILED (%d/%d passed)%s\n' "$BOLD" "$RED" "$fail" "$pass" "$total" "$RESET"
    exit 1
fi
