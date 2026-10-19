#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <arpa/inet.h>
#include <jansson.h>

#include "lsdb.h"
#include "return.h"

static int mask_prefix_len(const char *mask)
{
    struct in_addr addr;
    if (!mask || inet_pton(AF_INET, mask, &addr) != 1)
        return -1;
    uint32_t m = ntohl((uint32_t)addr.s_addr);
    int bits = 0;
    while (m & 0x80000000u) { bits++; m <<= 1; }
    return bits;
}

static void show_neighbors(void)
{
    json_error_t error;
    json_t *root = json_load_file(JSON_FILE_NAME, 0, &error);
    if (!root)
    {
        fprintf(stderr, "Error reading %s: %s\n", JSON_FILE_NAME, error.text);
        return;
    }

    const char *my_name = lsdb_jstr(json_object_get(root, "name"), "unknown");
    printf("Router: %s\n\n", my_name);

    json_t *neighbors = json_object_get(root, "neighbors");
    if (!neighbors || !json_is_array(neighbors) || json_array_size(neighbors) == 0)
    {
        printf("No OSPF neighbor known.\n");
        json_decref(root);
        return;
    }

    /* Deduplicate by next_hop to show one line per neighbor router */
    printf("%-20s %-20s %-10s %s\n", "Router name", "Next-hop IP", "Hops", "Interface");
    printf("%-20s %-20s %-10s %s\n",
           "--------------------", "--------------------", "----------", "---------");

    char seen[64][INET_ADDRSTRLEN];
    int seen_count = 0;

    size_t index;
    json_t *value;
    json_array_foreach(neighbors, index, value)
    {
        const char *name     = lsdb_jstr(json_object_get(value, "name"), "?");
        const char *next_hop = lsdb_jstr(json_object_get(value, "next_hop"), NULL);
        const char *gateway  = lsdb_jstr(json_object_get(value, "gateway"), "?");
        int hop              = (int)json_integer_value(json_object_get(value, "hop"));

        if (!next_hop) continue;

        int already = 0;
        for (int i = 0; i < seen_count; i++)
        {
            if (strcmp(seen[i], next_hop) == 0) { already = 1; break; }
        }
        if (already) continue;

        if (seen_count < 64)
        {
            strncpy(seen[seen_count], next_hop, INET_ADDRSTRLEN - 1);
            seen[seen_count][INET_ADDRSTRLEN - 1] = '\0';
            seen_count++;
        }

        printf("%-20s %-20s %-10d %s\n", name, next_hop, hop, gateway);
    }

    json_decref(root);
}

/* Effective routing table: one line per network, the best path only — the
 * same data the daemon actually programs into the kernel. */
static void show_routes(void)
{
    Route table[MAX_ROUTES];
    int count = generate_routing_table_from_file(JSON_FILE_NAME, table);
    if (count < 0)
    {
        fprintf(stderr, "Error reading %s\n", JSON_FILE_NAME);
        return;
    }

    printf("%-20s %-16s %-10s %s\n", "Destination", "Next hop", "Interface", "Hops");
    printf("%-20s %-16s %-10s %s\n",
           "--------------------", "----------------", "----------", "----");

    for (int i = 0; i < count; i++)
    {
        char dest[40];
        int prefix = mask_prefix_len(table[i].mask);
        snprintf(dest, sizeof(dest), "%s/%d", table[i].network, prefix);
        const char *next_hop = table[i].is_connected ? "-" : table[i].next_hop;
        printf("%-20s %-16s %-10s %d\n", dest, next_hop, table[i].gateway, table[i].hop);
    }
}

/* Raw LSDB dump: every entry as received, including duplicate next-hops
 * for the same network (useful to see the equal-cost paths before they
 * get collapsed into the effective table above). */
static void show_lsdb(void)
{
    json_error_t error;
    json_t *root = json_load_file(JSON_FILE_NAME, 0, &error);
    if (!root)
    {
        fprintf(stderr, "Error reading %s: %s\n", JSON_FILE_NAME, error.text);
        return;
    }

    printf("%-20s %-20s %-20s %-10s %s\n",
           "Network", "Mask", "Next-hop", "Hops", "Interface");
    printf("%-20s %-20s %-20s %-10s %s\n",
           "--------------------", "--------------------",
           "--------------------", "----------", "---------");

    json_t *connected = json_object_get(root, "connected");
    if (json_is_array(connected))
    {
        size_t i;
        json_t *e;
        json_array_foreach(connected, i, e)
        {
            printf("%-20s %-20s %-20s %-10d %s\n",
                   lsdb_jstr(json_object_get(e, "network"), "?"),
                   lsdb_jstr(json_object_get(e, "mask"), "?"),
                   "directly connected",
                   (int)json_integer_value(json_object_get(e, "hop")),
                   lsdb_jstr(json_object_get(e, "gateway"), "?"));
        }
    }

    json_t *neighbors = json_object_get(root, "neighbors");
    if (json_is_array(neighbors))
    {
        size_t i;
        json_t *e;
        json_array_foreach(neighbors, i, e)
        {
            printf("%-20s %-20s %-20s %-10d %s\n",
                   lsdb_jstr(json_object_get(e, "network"), "?"),
                   lsdb_jstr(json_object_get(e, "mask"), "?"),
                   lsdb_jstr(json_object_get(e, "next_hop"), "?"),
                   (int)json_integer_value(json_object_get(e, "hop")),
                   lsdb_jstr(json_object_get(e, "gateway"), "?"));
        }
    }

    json_decref(root);
}

int main(int argc, char *argv[])
{
    if (argc == 2 && strcmp(argv[1], "neighbors") == 0)
    {
        show_neighbors();
        return 0;
    }
    if (argc == 2 && strcmp(argv[1], "routes") == 0)
    {
        show_routes();
        return 0;
    }
    if (argc == 2 && strcmp(argv[1], "lsdb") == 0)
    {
        show_lsdb();
        return 0;
    }
    printf("Usage: %s [neighbors|routes|lsdb]\n", argv[0]);
    printf("  neighbors  known OSPF neighbor routers\n");
    printf("  routes     effective routing table (best path per network)\n");
    printf("  lsdb       raw link-state database (all paths, including non-best)\n");
    return 1;
}
