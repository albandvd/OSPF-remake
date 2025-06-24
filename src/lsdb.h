#ifndef LSDB_H
#define LSDB_H

#include "return.h"
#include <jansson.h>
#include <net/if.h>

#define MAX_LSAS 10 // Tableau de 10 LSDA nb nécessaire pour les tests

#define PORT 4242
#define JSON_FILE_NAME "lsdb.json"

struct Interface{
    char nameInterface[IFNAMSIZ];
    char ip[16];
    char mask[16];
    char network[16];
    char mac[18];
};
typedef struct Interface Interface;

struct LSA{
    char routerName[3];
    int routerID; 
    Interface interfaces; 
};
typedef struct LSA LSA;

struct Route {
    char network[20];
    char mask[20];
    char gateway[20];
    char next_hop[20];
    int hop;
};
typedef struct Route Route;

struct LSDB{
    int countLSA;
    LSA lsa[MAX_LSAS];
};
typedef struct LSDB LSDB;

ReturnCode retrieve_lsdb(LSDB *lsdb);
ReturnCode init_json(const char *output_file);
ReturnCode print_json_connected(const char *output_file);
ReturnCode route_exists(json_t *array, const char *network, const char *mask);
const ReturnCode *find_gateway_interface(json_t *connected, const char *ip);
ReturnCode print_json_neighbors(const char *my_json_file, const char *peer_ip, const char *peer_json_text);
ReturnCode save_lsdb(LSDB *lsdb);
ReturnCode add_lsa(LSA *lsa, LSDB *lsdb);
ReturnCode remove_lsa(const char *nameRouter, const char *nameInterface, LSDB *lsdb);
ReturnCode get_routerId(int *routerID);

#endif