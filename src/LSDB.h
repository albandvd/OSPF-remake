#ifndef LSDB_H
#define LSDB_H

#define IFNAMSIZ 16
#define MAX_LSAS 10 // Tableau de 10 LSDA nb nécessaire pour les tests

struct Interface{
    char nameInterface[IFNAMSIZ];
    char ip[16];
    char mask[16];
    char network[16];
    char mac[18];
    int idRouter;
};
typedef struct Interface Interface;

struct LSA{
    char nameRouter[3];
    int numRouter;
    Interface interfaces; 
};
typedef struct LSA LSA;

struct LSDB{
    int numRouter;
    int countLSA;
    LSA lsda[MAX_LSAS];
};
typedef struct LSDB LSDB;

ReturnCode retrieve_lsdb(LSDB *lsdb);
ReturnCode save_lsdb(LSDB *lsdb);
ReturnCode add_lsa(LSA *lsa, LSDB *lsdb);
ReturnCode remove_lsa(const char *nameRouter, const char *nameInterface, LSDB *lsdb);

#endif