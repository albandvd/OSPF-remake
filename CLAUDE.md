# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

OSPF-remake is a lightweight dynamic routing daemon written in C for Linux, implementing a simplified alternative to OSPF (Open Shortest Path First). It uses hop count as its path metric, distributes topology via LSA (Link State Advertisements) over TCP, and programs the Linux kernel routing table directly. The target environment is Alpine Linux router containers.

## Build & Run

### Root Makefile (preferred)
```bash
make up      # docker compose up --build — also runs the demo scenario (profile "demo", on by default via .env)
make demo    # re-run just the scenario against an already-running lab
make logs    # follow logs from every container
make down    # stop and remove containers/networks
make clean   # full teardown: containers, networks, volumes, local images
```

### Production (5 routers + 3 terminals)
```bash
docker compose up -d --build       # Start full network topology (+ the demo scenario)
docker compose down                 # Tear down
docker exec -it r1 sh              # Connect to a router
```

### Demo scenario (`scenario/run.sh`)
Runs as its own Compose service (profile `demo`, enabled by default via `.env`'s
`COMPOSE_PROFILES=demo`). Talks to the other containers via the Docker socket mounted
read-only (sibling-container pattern — acceptable for this local lab, not a general-purpose
pattern to reuse elsewhere). Waits for convergence, prints routing tables, exercises the
ping matrix, crashes `r2` (sits on an equal-cost path to t3 and is the sole path to t2),
verifies failover and the expected t2 outage, restarts `r2`, verifies full recovery, and
prints a final PASS/FAIL summary with measured convergence times.

### Inside a router container
```bash
/opt/ospf/show neighbors           # Display known OSPF neighbors
ip route show                      # Inspect kernel routing table
cat /opt/ospf/lsdb.json            # Inspect LSDB state
```

### Development container
```bash
cd dev
docker build -t dev-ospf ./
docker run --name dev --rm -it --cap-add=NET_ADMIN dev-ospf
```

### Manual compile (inside container or with jansson installed)
```bash
cd src
make                               # Builds: main, interface, show
make clean
```

Compiler flags: `gcc -Wall -Wextra -g`, linked with `-ljansson`.

## Architecture

### Components

**`src/main.c`** — Entry point. Calls `lsdb_init()` to bootstrap the LSDB (creates `lsdb.json` from hostname), then triggers neighbor discovery and route computation.

**`src/lsdb.c/h`** — Core Link State Database. Holds the network topology as a JSON structure (`lsdb.json`). Responsible for merging received LSAs, computing shortest paths (hop count), and triggering route updates. This is the most critical file.

**`src/exchanges.c/h`** — TCP networking layer. Serializes/deserializes the LSDB as JSON and exchanges it with neighbors on **port 4242**. Implements both client (send) and server (receive) sides.

**`src/interface.c`** — Interface discovery using `getifaddrs()`. Discovers active interfaces, derives subnet ranges, and scans each subnet for OSPF-capable neighbors by attempting TCP connections.

**`src/route.c/h`** — Linux kernel routing table manipulation. Validates every field (`inet_pton` for IPs/masks, `if_nametoindex` for the interface) then runs `ip route replace|del` via `fork`+`execvp` with an explicit `argv` — no shell is ever invoked, since these fields can originate from a neighbor's LSA over the network.

**`src/show.c`** — CLI utility: `show routes` prints the effective routing table (best path per network, computed from `lsdb.json`), `show lsdb` prints the raw LSDB (including non-best equal-cost entries), `show neighbors` prints known neighbor routers.

**`src/log.c/h`** — Pedagogical event logger (`log_event`/`log_debug`), line-buffered so `docker logs` shows output immediately. Format: `[HH:MM:SS] [hostname] EVENT message`. `LOG_LEVEL=debug` re-enables verbose per-IP scan logging.

**`src/return.c/h`** — Enum-based return codes (`ReturnCode`) used throughout. Every function returns a `ReturnCode`; never use raw errno directly.

**`src/check-service.c/h`** — Health/service verification.

### Data Flow

```
Startup:
  main.c → lsdb_init()       (create lsdb.json with hostname)
         → interface.c        (enumerate interfaces, derive subnets)
         → exchanges.c        (scan each subnet, TCP connect to neighbors)
         → lsdb.c             (merge received LSAs, compute shortest paths)
         → route.c            (program kernel routing table)
         → exchanges.c        (listen server loop for incoming LSAs)

Topology change:
  Neighbor sends updated LSA → lsdb.c merges → recompute paths → route.c reprograms kernel
```

### LSDB JSON format (`/opt/ospf/lsdb.json`, absolute path — `JSON_FILE_NAME` in `lsdb.h`)

```json
{
  "name": "r1",
  "sequence": 1,
  "connected": [
    { "network": "10.1.0.0", "mask": "255.255.255.0", "gateway": "eth0", "hop": 0, "is_Ospf": 1 }
  ],
  "neighbors": [
    { "name": "r2", "network": "10.2.0.0", "mask": "255.255.255.0",
      "gateway": "eth0", "next_hop": "10.1.0.2", "hop": 1, "last_seen": 1732000000 }
  ]
}
```

Hop convention: `0` = directly connected, `1` = one router away, etc. (`HOP_CONNECTED` in `lsdb.h`).
`connected[].gateway` is always a local interface name; `neighbors[].gateway` is the local interface
used to reach that neighbor, while `neighbors[].next_hop` is the neighbor's IP. `last_seen` (unix
timestamp) drives `prune_stale_neighbors()`: entries not refreshed within `NEIGHBOR_MAX_AGE_SECONDS`
are dropped individually — the daemon no longer wipes the whole neighbors array every cycle, since
doing so made a router briefly report "no neighbors" to anyone querying it mid-rescan.

### Network Topology (docker-compose.yml)

5 routers (R1–R5) interconnected across 3 core networks (10.1.0.0/24, 10.2.0.0/24, 10.3.0.0/24) and 3 access networks (192.168.1–3.0/24). 3 terminal nodes (T_A1, T_A2, T_A3) attached to access networks. Router containers only need `NET_ADMIN` (routing table changes; the sysctls in `sysctls:` are applied declaratively by Compose itself, no extra capability needed for those).

Per-router interface configuration lives in `entrypoint.sh`, which branches on `$HOSTNAME` to call `/opt/ospf/interface add <iface>` for each core-network interface. It also removes the default route Docker injects via the bridge gateway on every router, so only protocol-computed routes decide where router-originated traffic goes (terminals keep theirs, set explicitly in `docker-compose.yml`).

Router images build via a multi-stage `Dockerfile` (alpine 3.20: a builder stage with the toolchain, a runtime stage with just the binaries + `jansson` + `iproute2`/`iputils`). `Dockerfile.terminal` (Debian) adds `traceroute` for the demo scenario's path-visualization step.

## Conventions

- **Function naming:** prefixed by module — `lsdb_*`, `route_*`, `exchanges_*`, `check_*`
- **Return codes:** always use `ReturnCode` enum from `return.h`; `RETURN_SUCCESS` on success
- **Constants:** uppercase — `PORT` (4242), `JSON_FILE_NAME`, `MAX_ROUTES`
- **No GUI:** CLI-only, display via `show` binary
- **Linux-only:** uses `ioctl`, `rtentry`, `getifaddrs` — not portable

## Key Constraints (from docs/specification.pdf / docs/specification-fr.md)

Priority 1 (must implement): path computation by hop count, topology update on change, enable/disable per interface, IPv4 routing table update, neighbor list storage and display, failure tolerance.

Priority 2 (optimization): minimize exchanges, memory, and convergence time; default route origination.

Priority 3 (advanced): bandwidth-aware metrics, exchange authentication/encryption.
