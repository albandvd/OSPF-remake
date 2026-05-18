#include <unistd.h>
#include <limits.h>
#include <sys/types.h>
#include <ifaddrs.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <net/if.h>
#include <sys/socket.h>

#include "lsdb.h"
#include "return.h"

#define MAX_HOPS 15   /* routes beyond this hop count are treated as unreachable */

/* Atomic JSON write: write to tmpfile then rename to prevent corruption */
static int dump_json_atomic(json_t *root, const char *filename)
{
    char tmp[256];
    snprintf(tmp, sizeof(tmp), "%s.tmp%d", filename, (int)getpid());
    if (json_dump_file(root, tmp, JSON_INDENT(4)) != 0)
        return -1;
    return rename(tmp, filename);
}

/* Return 1 if ip is one of our own interface addresses (for split-horizon) */
static int is_our_own_ip(const char *ip)
{
    struct ifaddrs *ifap, *ifa;
    if (getifaddrs(&ifap) != 0)
        return 0;
    int found = 0;
    for (ifa = ifap; ifa != NULL; ifa = ifa->ifa_next)
    {
        if (!ifa->ifa_addr || ifa->ifa_addr->sa_family != AF_INET)
            continue;
        struct sockaddr_in *sa = (struct sockaddr_in *)ifa->ifa_addr;
        char my_ip[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &sa->sin_addr, my_ip, INET_ADDRSTRLEN);
        if (strcmp(my_ip, ip) == 0) { found = 1; break; }
    }
    freeifaddrs(ifap);
    return found;
}

ReturnCode init_json(const char *output_file)
{
    json_t *root = json_object();
    if (!root)
        return JSON_DUMP_ERROR;

    char hostname[HOST_NAME_MAX + 1];
    if (gethostname(hostname, sizeof(hostname)) != 0)
    {
        perror("gethostname");
        json_decref(root);
        return HOSTNAME_ERROR;
    }

    json_object_set_new(root, "name", json_string(hostname));
    json_object_set_new(root, "connected", json_array());
    json_object_set_new(root, "neighbors", json_array());
    json_object_set_new(root, "sequence", json_integer(1));

    if (dump_json_atomic(root, output_file) != 0)
    {
        json_decref(root);
        return JSON_WRITE_ERROR;
    }

    json_decref(root);
    return JSON_WRITE_SUCCESS;
}

ReturnCode print_json_connected(const char *output_file)
{
    json_error_t error;
    json_t *root = json_load_file(output_file, 0, &error);
    if (!root)
    {
        fprintf(stderr, "Erreur ouverture JSON '%s': %s\n", output_file, error.text);
        return JSON_LOAD_ERROR;
    }

    /* Preserve existing is_Ospf flags before rebuilding connected array */
    json_t *old_connected = json_object_get(root, "connected");
    json_t *ospf_flags = json_object();  /* interface_name -> is_Ospf value */
    if (old_connected && json_is_array(old_connected))
    {
        size_t k;
        json_t *oc;
        json_array_foreach(old_connected, k, oc)
        {
            const char *gw = json_string_value(json_object_get(oc, "gateway"));
            json_t *flag = json_object_get(oc, "is_Ospf");
            if (gw && flag)
                json_object_set(ospf_flags, gw, flag);
        }
    }

    /* Rebuild connected array from live interfaces */
    json_t *connected = json_array();

    struct ifaddrs *ifap = NULL, *ifa = NULL;
    if (getifaddrs(&ifap) != 0)
    {
        perror("getifaddrs");
        json_decref(ospf_flags);
        json_decref(root);
        return INTERFACE_NOT_FOUND;
    }

    for (ifa = ifap; ifa != NULL; ifa = ifa->ifa_next)
    {
        if (!ifa->ifa_addr || ifa->ifa_addr->sa_family != AF_INET)
            continue;
        if (strcmp(ifa->ifa_name, "lo") == 0)
            continue;

        struct sockaddr_in *sa = (struct sockaddr_in *)ifa->ifa_addr;
        struct sockaddr_in *nm = (struct sockaddr_in *)ifa->ifa_netmask;
        char netmask[INET_ADDRSTRLEN] = "N/A";
        char network[INET_ADDRSTRLEN] = "N/A";

        if (ifa->ifa_netmask)
        {
            strncpy(netmask, inet_ntoa(nm->sin_addr), INET_ADDRSTRLEN - 1);
            struct in_addr net_addr;
            net_addr.s_addr = sa->sin_addr.s_addr & nm->sin_addr.s_addr;
            strncpy(network, inet_ntoa(net_addr), INET_ADDRSTRLEN - 1);
        }

        json_t *entry = json_object();
        json_object_set_new(entry, "network", json_string(network));
        json_object_set_new(entry, "mask", json_string(netmask));
        json_object_set_new(entry, "gateway", json_string(ifa->ifa_name));
        json_object_set_new(entry, "hop", json_integer(1));

        /* Restore is_Ospf flag if it was set before */
        json_t *prev_flag = json_object_get(ospf_flags, ifa->ifa_name);
        json_object_set_new(entry, "is_Ospf",
                            json_integer(prev_flag ? json_integer_value(prev_flag) : 0));

        json_array_append_new(connected, entry);
    }
    freeifaddrs(ifap);

    json_object_set(root, "connected", connected);
    json_decref(connected);
    json_decref(ospf_flags);

    if (dump_json_atomic(root, output_file) != 0)
    {
        json_decref(root);
        return JSON_DUMP_ERROR;
    }

    json_decref(root);
    return JSON_WRITE_SUCCESS;
}

/* Returns 1 if network/mask already in array, 0 otherwise */
ReturnCode route_exists(json_t *array, const char *network, const char *mask)
{
    size_t i;
    json_t *elem;
    json_array_foreach(array, i, elem)
    {
        const char *net = json_string_value(json_object_get(elem, "network"));
        const char *msk = json_string_value(json_object_get(elem, "mask"));
        if (net && msk && strcmp(net, network) == 0 && strcmp(msk, mask) == 0)
            return INTERFACE_NOT_FOUND;  /* reuse code meaning "already exists" */
    }
    return RETURN_SUCCESS;
}

/* Returns existing neighbor entry matching (network, mask, gateway, next_hop), or NULL */
static json_t *find_neighbor_entry(json_t *array, const char *network, const char *mask,
                                   const char *gateway, const char *next_hop)
{
    size_t i;
    json_t *elem;
    json_array_foreach(array, i, elem)
    {
        const char *net = json_string_value(json_object_get(elem, "network"));
        const char *msk = json_string_value(json_object_get(elem, "mask"));
        const char *gw  = json_string_value(json_object_get(elem, "gateway"));
        const char *nh  = json_string_value(json_object_get(elem, "next_hop"));
        if (net && msk && gw && nh &&
            strcmp(net, network) == 0 &&
            strcmp(msk, mask) == 0 &&
            strcmp(gw, gateway) == 0 &&
            strcmp(nh, next_hop) == 0)
            return elem;
    }
    return NULL;
}

int neighbor_entry_exists(json_t *array, const char *network, const char *mask,
                          const char *gateway, const char *next_hop)
{
    return find_neighbor_entry(array, network, mask, gateway, next_hop) != NULL;
}

/* Find the local interface name used to reach a given IP */
const char *find_gateway_interface(json_t *connected, const char *ip)
{
    size_t i;
    json_t *entry;
    struct in_addr addr_ip, addr_net, addr_mask, addr_calc;

    if (!inet_aton(ip, &addr_ip))
        return NULL;

    json_array_foreach(connected, i, entry)
    {
        const char *network = json_string_value(json_object_get(entry, "network"));
        const char *mask    = json_string_value(json_object_get(entry, "mask"));
        const char *gateway = json_string_value(json_object_get(entry, "gateway"));
        if (!network || !mask || !gateway)
            continue;

        inet_aton(network, &addr_net);
        inet_aton(mask, &addr_mask);
        addr_calc.s_addr = addr_ip.s_addr & addr_mask.s_addr;
        if (addr_calc.s_addr == addr_net.s_addr)
            return gateway;
    }
    return NULL;
}

ReturnCode print_json_neighbors(const char *my_json_file, const char *peer_ip,
                                const char *peer_json_text)
{
    if (!peer_json_text)
        return ERROR_INVALID_ARGUMENT;

    json_error_t error;
    json_t *my_root   = json_load_file(my_json_file, 0, &error);
    json_t *peer_root = json_loads(peer_json_text, 0, &error);

    if (!my_root || !peer_root)
    {
        fprintf(stderr, "Erreur chargement JSON: %s\n", error.text);
        if (my_root)   json_decref(my_root);
        if (peer_root) json_decref(peer_root);
        return RETURN_UNKNOWN_ERROR;
    }

    json_t *my_connected  = json_object_get(my_root, "connected");
    json_t *my_neighbors  = json_object_get(my_root, "neighbors");
    json_t *peer_connected = json_object_get(peer_root, "connected");
    json_t *peer_neighbors = json_object_get(peer_root, "neighbors");

    if (!json_is_array(my_connected) || !json_is_array(my_neighbors) ||
        !json_is_array(peer_connected))
    {
        json_decref(my_root);
        json_decref(peer_root);
        return RETURN_UNKNOWN_ERROR;
    }

    const char *peer_name = json_string_value(json_object_get(peer_root, "name"));
    if (!peer_name) peer_name = peer_ip;

    const char *gateway = find_gateway_interface(my_connected, peer_ip);
    if (!gateway)
    {
        json_decref(my_root);
        json_decref(peer_root);
        return RETURN_SUCCESS;
    }

    int modification = 0;

    /* Add peer's directly connected networks (hop = 2) */
    size_t i;
    json_t *pr;
    json_array_foreach(peer_connected, i, pr)
    {
        const char *network = json_string_value(json_object_get(pr, "network"));
        const char *mask    = json_string_value(json_object_get(pr, "mask"));
        if (!network || !mask)
            continue;

        /* Skip networks we are already directly connected to */
        if (route_exists(my_connected, network, mask) != RETURN_SUCCESS)
            continue;

        json_t *existing = find_neighbor_entry(my_neighbors, network, mask, gateway, peer_ip);
        if (existing)
        {
            int cur_hop = (int)json_integer_value(json_object_get(existing, "hop"));
            if (2 < cur_hop)
            {
                json_object_set(existing, "hop", json_integer(2));
                modification = 1;
            }
            continue;
        }

        json_t *nb = json_object();
        json_object_set_new(nb, "name",     json_string(peer_name));
        json_object_set_new(nb, "network",  json_string(network));
        json_object_set_new(nb, "mask",     json_string(mask));
        json_object_set_new(nb, "gateway",  json_string(gateway));
        json_object_set_new(nb, "hop",      json_integer(2));
        json_object_set_new(nb, "next_hop", json_string(peer_ip));
        json_array_append_new(my_neighbors, nb);
        modification = 1;
    }

    /* Add peer's known routes (hop = their_hop + 1) */
    if (json_is_array(peer_neighbors))
    {
        size_t j;
        json_t *pn;
        json_array_foreach(peer_neighbors, j, pn)
        {
            const char *network      = json_string_value(json_object_get(pn, "network"));
            const char *mask         = json_string_value(json_object_get(pn, "mask"));
            const char *pn_name      = json_string_value(json_object_get(pn, "name"));
            const char *peer_next_hop = json_string_value(json_object_get(pn, "next_hop"));
            int peer_hop             = (int)json_integer_value(json_object_get(pn, "hop"));
            if (!network || !mask)
                continue;

            /* Hop limit: discard routes that are too far */
            if (peer_hop >= MAX_HOPS)
                continue;

            /* Split horizon: the peer learned this route from us — don't re-learn it */
            if (peer_next_hop && is_our_own_ip(peer_next_hop))
                continue;

            /* Skip networks we are directly connected to */
            if (route_exists(my_connected, network, mask) != RETURN_SUCCESS)
                continue;

            int new_hop = peer_hop + 1;
            json_t *existing = find_neighbor_entry(my_neighbors, network, mask, gateway, peer_ip);
            if (existing)
            {
                int cur_hop = (int)json_integer_value(json_object_get(existing, "hop"));
                if (new_hop < cur_hop)
                {
                    json_object_set(existing, "hop", json_integer(new_hop));
                    modification = 1;
                }
                continue;
            }

            json_t *nb = json_object();
            json_object_set_new(nb, "name",     json_string(pn_name ? pn_name : peer_name));
            json_object_set_new(nb, "network",  json_string(network));
            json_object_set_new(nb, "mask",     json_string(mask));
            json_object_set_new(nb, "gateway",  json_string(gateway));
            json_object_set_new(nb, "hop",      json_integer(new_hop));
            json_object_set_new(nb, "next_hop", json_string(peer_ip));
            json_array_append_new(my_neighbors, nb);
            modification = 1;
        }
    }

    if (modification)
    {
        json_t *seq = json_object_get(my_root, "sequence");
        int sequence = seq ? (int)json_integer_value(seq) : 1;
        json_object_set(my_root, "sequence", json_integer(sequence + 1));
    }

    dump_json_atomic(my_root, my_json_file);
    json_decref(my_root);
    json_decref(peer_root);
    return RETURN_SUCCESS;
}

ReturnCode set_is_ospf(const char *json_file, const char *interface_name)
{
    json_error_t error;
    json_t *root = json_load_file(json_file, 0, &error);
    if (!root)
        return JSON_LOAD_ERROR;

    json_t *connected = json_object_get(root, "connected");
    if (!connected || !json_is_array(connected))
    {
        json_decref(root);
        return JSON_LOAD_ERROR;
    }

    size_t i;
    json_t *entry;
    int found = 0;
    json_array_foreach(connected, i, entry)
    {
        const char *gateway = json_string_value(json_object_get(entry, "gateway"));
        if (gateway && strcmp(gateway, interface_name) == 0)
        {
            json_object_set_new(entry, "is_Ospf", json_integer(1));
            found = 1;
            break;
        }
    }

    if (!found)
    {
        json_decref(root);
        return INTERFACE_NOT_FOUND;
    }

    json_t *seq = json_object_get(root, "sequence");
    int sequence = seq ? (int)json_integer_value(seq) : 1;
    json_object_set(root, "sequence", json_integer(sequence + 1));

    if (dump_json_atomic(root, json_file) != 0)
    {
        json_decref(root);
        return JSON_DUMP_ERROR;
    }
    json_decref(root);
    return RETURN_SUCCESS;
}

ReturnCode unset_is_ospf(const char *json_file, const char *interface_name)
{
    json_error_t error;
    json_t *root = json_load_file(json_file, 0, &error);
    if (!root)
        return JSON_LOAD_ERROR;

    json_t *connected = json_object_get(root, "connected");
    if (!connected || !json_is_array(connected))
    {
        json_decref(root);
        return JSON_LOAD_ERROR;
    }

    size_t i;
    json_t *entry;
    int found = 0;
    json_array_foreach(connected, i, entry)
    {
        const char *gateway = json_string_value(json_object_get(entry, "gateway"));
        if (gateway && strcmp(gateway, interface_name) == 0)
        {
            json_object_set_new(entry, "is_Ospf", json_integer(0));
            found = 1;
            break;
        }
    }

    if (!found)
    {
        json_decref(root);
        return INTERFACE_NOT_FOUND;
    }

    if (dump_json_atomic(root, json_file) != 0)
    {
        json_decref(root);
        return JSON_DUMP_ERROR;
    }
    json_decref(root);
    return RETURN_SUCCESS;
}

ReturnCode clear_neighbors(const char *json_file)
{
    json_error_t error;
    json_t *root = json_load_file(json_file, 0, &error);
    if (!root)
        return JSON_LOAD_ERROR;

    json_object_set_new(root, "neighbors", json_array());

    if (dump_json_atomic(root, json_file) != 0)
    {
        json_decref(root);
        return JSON_DUMP_ERROR;
    }
    json_decref(root);
    return RETURN_SUCCESS;
}

int get_ospf_interfaces(const char *json_file, char interfaces[][IFNAMSIZ], int max_count)
{
    json_error_t error;
    json_t *root = json_load_file(json_file, 0, &error);
    if (!root)
        return 0;

    json_t *connected = json_object_get(root, "connected");
    if (!connected || !json_is_array(connected))
    {
        json_decref(root);
        return 0;
    }

    int count = 0;
    size_t i;
    json_t *entry;
    json_array_foreach(connected, i, entry)
    {
        if (count >= max_count)
            break;
        json_t *flag = json_object_get(entry, "is_Ospf");
        if (!flag || json_integer_value(flag) == 0)
            continue;
        const char *gw = json_string_value(json_object_get(entry, "gateway"));
        if (gw)
        {
            strncpy(interfaces[count], gw, IFNAMSIZ - 1);
            interfaces[count][IFNAMSIZ - 1] = '\0';
            count++;
        }
    }

    json_decref(root);
    return count;
}

int route_exists_and_is_better(Route *routes, int count, Route *new_route)
{
    for (int i = 0; i < count; ++i)
    {
        if (strcmp(routes[i].network, new_route->network) == 0 &&
            strcmp(routes[i].mask, new_route->mask) == 0)
        {
            if (new_route->hop < routes[i].hop)
                routes[i] = *new_route;
            return 1;
        }
    }
    return 0;
}

int generate_routing_table_from_file(const char *filename, Route *routing_table)
{
    json_error_t error;
    json_t *root = json_load_file(filename, 0, &error);
    if (!root)
    {
        fprintf(stderr, "Erreur lecture JSON: %s\n", error.text);
        return -1;
    }

    int count = 0;

    json_t *connected = json_object_get(root, "connected");
    if (!json_is_array(connected))
    {
        json_decref(root);
        return -1;
    }

    size_t i;
    json_t *entry;
    json_array_foreach(connected, i, entry)
    {
        Route r;
        memset(&r, 0, sizeof(r));
        snprintf(r.network,  sizeof(r.network),  "%s",
                 json_string_value(json_object_get(entry, "network")));
        snprintf(r.mask,     sizeof(r.mask),     "%s",
                 json_string_value(json_object_get(entry, "mask")));
        snprintf(r.gateway,  sizeof(r.gateway),  "%s",
                 json_string_value(json_object_get(entry, "gateway")));
        r.hop = (int)json_integer_value(json_object_get(entry, "hop"));
        r.is_connected = 1;
        routing_table[count++] = r;
        if (count >= MAX_ROUTES) break;
    }

    json_t *neighbors = json_object_get(root, "neighbors");
    if (json_is_array(neighbors))
    {
        json_array_foreach(neighbors, i, entry)
        {
            if (count >= MAX_ROUTES) break;
            Route r;
            memset(&r, 0, sizeof(r));
            snprintf(r.network,  sizeof(r.network),  "%s",
                     json_string_value(json_object_get(entry, "network")));
            snprintf(r.mask,     sizeof(r.mask),     "%s",
                     json_string_value(json_object_get(entry, "mask")));
            snprintf(r.gateway,  sizeof(r.gateway),  "%s",
                     json_string_value(json_object_get(entry, "gateway")));
            snprintf(r.next_hop, sizeof(r.next_hop), "%s",
                     json_string_value(json_object_get(entry, "next_hop")));
            r.hop = (int)json_integer_value(json_object_get(entry, "hop"));
            r.is_connected = 0;

            if (!route_exists_and_is_better(routing_table, count, &r))
                routing_table[count++] = r;
        }
    }

    json_decref(root);
    return count;
}
