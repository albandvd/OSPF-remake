#ifndef LSDB_H
#define LSDB_H

#include "return.h"
#include <jansson.h>
#include <net/if.h>

#define PORT 4242
#define JSON_FILE_NAME "/opt/ospf/lsdb.json"
#define LOCK_FILE_NAME "/opt/ospf/lsdb.lock"
#define MAX_ROUTES 100
#define OSPF_INTERVAL 5  /* seconds between neighbor re-discovery cycles */
#define MAX_IFACE 16

/* Hop metric convention (kept consistent everywhere — JSON, show, logs):
 *   0 = directly connected network (no router in between)
 *   1 = network owned by a directly adjacent router
 *   N = network N routers away
 */
#define HOP_CONNECTED 0

/* Stats about a merge, filled in by print_json_neighbors(). Lets the
 * caller log something useful ("LSDB received from r2: seq 4, 6 entries,
 * 2 changes") without print_json_neighbors itself knowing about logging. */
typedef struct
{
    char peer_name[64];
    int peer_sequence;
    int entry_count;   /* total entries in the peer's LSDB (connected + neighbors) */
    int changes;        /* number of entries added or improved locally */
} MergeStats;

typedef struct
{
    char network[32];
    char mask[32];
    char gateway[IFNAMSIZ];
    char next_hop[32];
    int hop;
    int is_connected;
    int is_Ospf;
} Route;

const char *lsdb_jstr(json_t *value, const char *fallback);

ReturnCode init_json(const char *output_file);
ReturnCode print_json_connected(const char *output_file);
ReturnCode route_exists(json_t *array, const char *network, const char *mask);
const char *find_gateway_interface(json_t *connected, const char *ip);
ReturnCode print_json_neighbors(const char *my_json_file, const char *peer_ip,
                                const char *peer_json_text, MergeStats *stats);
ReturnCode set_is_ospf(const char *json_file, const char *interface_name);
ReturnCode unset_is_ospf(const char *json_file, const char *interface_name);
ReturnCode clear_neighbors(const char *json_file);
int get_ospf_interfaces(const char *json_file, char interfaces[][IFNAMSIZ], int max_count);
int is_interface_ospf_enabled(const char *json_file, const char *interface_name);
int neighbor_entry_exists(json_t *array, const char *network, const char *mask, const char *gateway, const char *next_hop);
int route_exists_and_is_better(Route *routes, int count, Route *new_route);
int generate_routing_table_from_file(const char *filename, Route *routing_table);
#endif