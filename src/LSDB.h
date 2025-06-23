#ifndef LSDB_H
#define LSDB_H

#include "return.h"

#define IFNAMSIZ 16
#define MAX_LSAS 10 // Tableau de 10 LSDA nb nécessaire pour les tests

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

struct LSDB{
    int countLSA;
    LSA lsa[MAX_LSAS];
};
typedef struct LSDB LSDB;

ReturnCode retrieve_lsdb(LSDB *lsdb);
ReturnCode save_lsdb(LSDB *lsdb);
ReturnCode add_lsa(LSA *lsa, LSDB *lsdb);
ReturnCode remove_lsa(const char *nameRouter, const char *nameInterface, LSDB *lsdb);
ReturnCode get_routerId(int *routerID);

#endif