#ifndef ROUTE_H
#define ROUTE_H

#include "return.h"

ReturnCode add_route(char network[16], char mask[16], char gateway[16], char interface[IFNAMSIZ]);
ReturnCode delete_route(char network[16], char mask[16], char gateway[16], char interface[IFNAMSIZ])

#endif
