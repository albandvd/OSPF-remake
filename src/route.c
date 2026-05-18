#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <net/if.h>

#include "route.h"
#include "lsdb.h"
#include "return.h"

/* Convert dotted-decimal netmask to CIDR prefix length */
static int mask_to_prefix(const char *mask)
{
    struct in_addr addr;
    if (!inet_aton(mask, &addr))
        return -1;
    uint32_t m = ntohl(addr.s_addr);
    int bits = 0;
    while (m & 0x80000000u) { bits++; m <<= 1; }
    return bits;
}

ReturnCode add_route(char network[16], char mask[16], char gateway[16],
                     char interface[IFNAMSIZ])
{
    int prefix = mask_to_prefix(mask);
    if (prefix < 0)
        return ROUTE_ADD_ERROR;

    char cmd[256];
    snprintf(cmd, sizeof(cmd),
             "ip route replace %s/%d via %s dev %s 2>/dev/null",
             network, prefix, gateway, interface);

    if (system(cmd) != 0)
        return ROUTE_ADD_ERROR;

    return ROUTE_ADDED_SUCCESSFULLY;
}

ReturnCode delete_route(char network[16], char mask[16], char gateway[16],
                        char interface[IFNAMSIZ])
{
    int prefix = mask_to_prefix(mask);
    if (prefix < 0)
        return ROUTE_DELETE_ERROR;

    char cmd[256];
    snprintf(cmd, sizeof(cmd),
             "ip route del %s/%d via %s dev %s 2>/dev/null",
             network, prefix, gateway, interface);

    /* Ignore errors — route may not exist in kernel */
    system(cmd);
    return ROUTE_DELETED_SUCCESSFULLY;
}

ReturnCode delete_all_ospf_routes(const char *json_file)
{
    Route table[MAX_ROUTES];
    int count = generate_routing_table_from_file(json_file, table);
    if (count < 0)
        return RETURN_SUCCESS;

    for (int i = 0; i < count; i++)
    {
        if (table[i].is_connected)
            continue;
        if (table[i].next_hop[0] == '\0')
            continue;
        delete_route(table[i].network, table[i].mask,
                     table[i].next_hop, table[i].gateway);
    }
    return RETURN_SUCCESS;
}
