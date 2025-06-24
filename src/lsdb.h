#ifndef LSDB_H
#define LSDB_H

#include "return.h"
#include <jansson.h>
#include <net/if.h>

#define PORT 4242
#define JSON_FILE_NAME "lsdb.json"

struct Route {
    char network[20];
    char mask[20];
    char gateway[20];
    char next_hop[20];
    int is_Ospf; // 1 if OSPF, 0 if not
    int hop;
};
typedef struct Route Route;

ReturnCode init_json(const char *output_file);
ReturnCode print_json_connected(const char *output_file);
ReturnCode route_exists(json_t *array, const char *network, const char *mask);
const ReturnCode *find_gateway_interface(json_t *connected, const char *ip);
ReturnCode print_json_neighbors(const char *my_json_file, const char *peer_ip, const char *peer_json_text);
ReturnCode set_is_ospf(const char *json_file, const char *interface_name);

#endif