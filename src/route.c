#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>
#include <fcntl.h>
#include <arpa/inet.h>
#include <net/if.h>
#include <sys/wait.h>

#include "route.h"
#include "lsdb.h"
#include "return.h"

/* Convert dotted-decimal netmask to CIDR prefix length, or -1 if invalid */
static int mask_to_prefix(const char *mask)
{
    struct in_addr addr;
    if (!mask || inet_pton(AF_INET, mask, &addr) != 1)
        return -1;
    uint32_t m = ntohl((uint32_t)addr.s_addr);
    int bits = 0;
    while (m & 0x80000000u) { bits++; m <<= 1; }
    return bits;
}

/* Run "ip <argv...>" via fork+execvp (no shell involved), so none of the
 * arguments — which may originate from a neighbor's LSA over the network —
 * can be interpreted as shell syntax. Returns 0 on a clean exit, -1
 * otherwise. */
static int run_ip_command(char *const argv[])
{
    pid_t pid = fork();
    if (pid < 0)
        return -1;

    if (pid == 0)
    {
        /* "ip route del" on a route that is already gone is expected during
         * normal operation; keep that noise out of the daemon's own logs. */
        int devnull = open("/dev/null", O_WRONLY);
        if (devnull >= 0)
        {
            dup2(devnull, STDERR_FILENO);
            dup2(devnull, STDOUT_FILENO);
            close(devnull);
        }
        execvp("ip", argv);
        _exit(127);
    }

    int status;
    if (waitpid(pid, &status, 0) < 0)
        return -1;
    return (WIFEXITED(status) && WEXITSTATUS(status) == 0) ? 0 : -1;
}

ReturnCode add_route(char network[32], char mask[32], char gateway[32],
                     char interface[IFNAMSIZ])
{
    struct in_addr tmp;
    int prefix = mask_to_prefix(mask);
    if (prefix < 0)
        return ROUTE_ADD_ERROR;
    if (inet_pton(AF_INET, network, &tmp) != 1)
        return ROUTE_ADD_ERROR;
    if (inet_pton(AF_INET, gateway, &tmp) != 1)
        return ROUTE_ADD_ERROR;
    if (if_nametoindex(interface) == 0)
        return ROUTE_ADD_ERROR;

    char dest[40];
    snprintf(dest, sizeof(dest), "%s/%d", network, prefix);

    char *argv[] = { (char *)"ip", (char *)"route", (char *)"replace",
                     dest, (char *)"via", gateway, (char *)"dev", interface, NULL };

    if (run_ip_command(argv) != 0)
        return ROUTE_ADD_ERROR;

    return ROUTE_ADDED_SUCCESSFULLY;
}

ReturnCode delete_route(char network[32], char mask[32], char gateway[32],
                        char interface[IFNAMSIZ])
{
    struct in_addr tmp;
    int prefix = mask_to_prefix(mask);
    if (prefix < 0)
        return ROUTE_DELETE_ERROR;
    if (inet_pton(AF_INET, network, &tmp) != 1)
        return ROUTE_DELETE_ERROR;
    if (inet_pton(AF_INET, gateway, &tmp) != 1)
        return ROUTE_DELETE_ERROR;
    if (if_nametoindex(interface) == 0)
        return ROUTE_DELETE_ERROR;

    char dest[40];
    snprintf(dest, sizeof(dest), "%s/%d", network, prefix);

    char *argv[] = { (char *)"ip", (char *)"route", (char *)"del",
                     dest, (char *)"via", gateway, (char *)"dev", interface, NULL };

    /* Ignore errors — the route may already be gone from the kernel table */
    run_ip_command(argv);
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
