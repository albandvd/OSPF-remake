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

typedef enum {
    LSDB_SUCCESS,
    LSDB_ERROR_FILE_NOT_FOUND,
    LSDB_ERROR_READ_FAILURE,
    LSDB_ERROR
} LSDBreturn;

#endif