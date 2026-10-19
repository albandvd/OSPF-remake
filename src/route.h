#ifndef ROUTE_H
#define ROUTE_H

#include "return.h"
#include <net/if.h>

ReturnCode add_route(char network[32], char mask[32], char gateway[32], char interface[IFNAMSIZ]);
ReturnCode delete_route(char network[32], char mask[32], char gateway[32], char interface[IFNAMSIZ]);
ReturnCode delete_all_ospf_routes(const char *json_file);

#endif // ROUTE_H
