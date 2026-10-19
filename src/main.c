#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <unistd.h>
#include <net/if.h>
#include <arpa/inet.h>

#include "lsdb.h"
#include "return.h"
#include "route.h"
#include "exchanges.h"
#include "log.h"

/* A full /24 sweep runs once at startup (automatically, since the known-
 * neighbor list starts empty — see exchanges.c) and then every N cycles to
 * pick up new neighbors; the cycles in between only re-contact neighbors
 * already known, which is what keeps failure detection fast. */
#define FULL_SCAN_EVERY_N_CYCLES 6

static Route *find_route(Route *table, int count, const char *network, const char *mask)
{
    for (int i = 0; i < count; i++)
        if (strcmp(table[i].network, network) == 0 && strcmp(table[i].mask, mask) == 0)
            return &table[i];
    return NULL;
}

/* Dotted netmask -> CIDR prefix length, for display only */
static int mask_prefix_len(const char *mask)
{
    struct in_addr addr;
    if (inet_pton(AF_INET, mask, &addr) != 1)
        return -1;
    uint32_t m = ntohl((uint32_t)addr.s_addr);
    int bits = 0;
    while (m & 0x80000000u) { bits++; m <<= 1; }
    return bits;
}

static void log_routing_table(Route *table, int count)
{
    log_event("ROUTING TABLE", "%d effective route%s", count, count == 1 ? "" : "s");
    printf("    %-20s %-16s %-10s %s\n", "Destination", "Next hop", "Interface", "Hops");
    for (int i = 0; i < count; i++)
    {
        char dest[40];
        int prefix = mask_prefix_len(table[i].mask);
        snprintf(dest, sizeof(dest), "%s/%d", table[i].network, prefix);
        const char *next_hop = table[i].is_connected ? "-" : table[i].next_hop;
        printf("    %-20s %-16s %-10s %d\n", dest, next_hop, table[i].gateway, table[i].hop);
    }
}

/* Compare the routing table before and after a discovery cycle and log a
 * pedagogical event for each transition — but reassert every non-connected
 * route in the kernel unconditionally (ip route replace is idempotent and
 * cheap), not just the ones this diff thinks changed. The forked server
 * process merges incoming LSAs into lsdb.json asynchronously, outside this
 * loop's own cycle boundary; a route it just learned can already be
 * identical in both the "old" and "new" snapshots taken here, so relying
 * on the diff alone to decide *whether* to program the kernel would
 * silently skip it. Returns the number of routes added, changed, or
 * removed (for deciding whether to print the full table). */
static int apply_route_changes(Route *old_table, int old_count,
                               Route *new_table, int new_count)
{
    int changes = 0;

    for (int i = 0; i < new_count; i++)
    {
        if (new_table[i].is_connected || new_table[i].next_hop[0] == '\0')
            continue;

        Route *old = find_route(old_table, old_count, new_table[i].network, new_table[i].mask);
        int prefix = mask_prefix_len(new_table[i].mask);
        ReturnCode rc = add_route(new_table[i].network, new_table[i].mask,
                                  new_table[i].next_hop, new_table[i].gateway);
        if (rc != ROUTE_ADDED_SUCCESSFULLY)
            continue;

        if (!old || old->is_connected)
        {
            log_event("ROUTE ADDED", "%s/%d via %s (%s) hop=%d",
                      new_table[i].network, prefix, new_table[i].next_hop,
                      new_table[i].gateway, new_table[i].hop);
            changes++;
            continue;
        }

        if (strcmp(old->next_hop, new_table[i].next_hop) != 0 || old->hop != new_table[i].hop)
        {
            log_event("ROUTE CHANGED", "%s/%d: via %s (hop=%d) -> via %s (hop=%d)",
                      new_table[i].network, prefix,
                      old->next_hop, old->hop,
                      new_table[i].next_hop, new_table[i].hop);
            changes++;
        }
    }

    for (int i = 0; i < old_count; i++)
    {
        if (old_table[i].is_connected || old_table[i].next_hop[0] == '\0')
            continue;

        Route *still_there = find_route(new_table, new_count, old_table[i].network, old_table[i].mask);
        if (still_there && !still_there->is_connected)
            continue;

        int prefix = mask_prefix_len(old_table[i].mask);
        delete_route(old_table[i].network, old_table[i].mask,
                     old_table[i].next_hop, old_table[i].gateway);
        log_event("ROUTE REMOVED", "%s/%d (was via %s)",
                  old_table[i].network, prefix, old_table[i].next_hop);
        changes++;
    }

    return changes;
}

int main(void)
{
    log_init();
    log_event("DAEMON START", "listening on TCP %d, cycle every %ds", PORT, OSPF_INTERVAL);

    init_json(JSON_FILE_NAME);
    print_json_connected(JSON_FILE_NAME);

    /* Fork the server process (listens on port PORT for incoming exchanges) */
    pid_t pid = fork();
    if (pid < 0)
    {
        perror("fork");
        return EXIT_FAILURE;
    }
    if (pid == 0)
    {
        log_init(); /* re-apply line buffering in the child; see log.h */
        run_server();   /* never returns */
        exit(0);
    }

    unsigned long cycle = 0;
    while (1)
    {
        sleep(OSPF_INTERVAL);

        Route old_routes[MAX_ROUTES];
        int old_count = generate_routing_table_from_file(JSON_FILE_NAME, old_routes);
        if (old_count < 0)
            old_count = 0;

        int full_scan = (cycle % FULL_SCAN_EVERY_N_CYCLES == 0);

        char interfaces[MAX_IFACE][IFNAMSIZ];
        int iface_count = get_ospf_interfaces(JSON_FILE_NAME, interfaces, MAX_IFACE);
        for (int i = 0; i < iface_count; i++)
            scan_subnet_for_ospf(interfaces[i], full_scan);
        prune_stale_neighbors(JSON_FILE_NAME, NEIGHBOR_MAX_AGE_SECONDS);

        Route new_routes[MAX_ROUTES];
        int new_count = generate_routing_table_from_file(JSON_FILE_NAME, new_routes);
        if (new_count < 0)
            new_count = 0;

        int changes = apply_route_changes(old_routes, old_count, new_routes, new_count);
        if (changes > 0)
            log_routing_table(new_routes, new_count);

        cycle++;
    }

    return RETURN_SUCCESS;
}
