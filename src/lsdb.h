#ifndef LSDB_H
#define LSDB_H

#include "return.h"
#include <jansson.h>
#include <net/if.h>

#define PORT 4242
#define JSON_FILE_NAME "lsdb.json"
#define MAX_ROUTES 100
typedef struct
{
    char network[32];
    char mask[32];
    char gateway[16];
    char next_hop[32];
    int hop;
    int is_connected;
    int is_Ospf
} Route;

ReturnCode init_json(const char *output_file);
ReturnCode print_json_connected(const char *output_file);
ReturnCode route_exists(json_t *array, const char *network, const char *mask);
const char *find_gateway_interface(json_t *connected, const char *ip);
ReturnCode print_json_neighbors(const char *my_json_file, const char *peer_ip, const char *peer_json_text);
ReturnCode set_is_ospf(const char *json_file, const char *interface_name);
ReturnCode send_json_to_ospf_neighbors(const char *json_file);
int neighbor_entry_exists(json_t *array, const char *network, const char *mask, const char *gateway, const char *next_hop);
int route_exists_and_is_better(Route *routes, int count, Route *new_route);
int generate_routing_table_from_file(const char *filename, Route *routing_table);
#endif