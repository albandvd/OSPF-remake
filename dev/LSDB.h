#define IFNAMSIZ 16

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
    LSA lsda[10]; // Tableau de 10 LSDA nb nécessaire pour les tests
};
typedef struct LSDB LSDB;