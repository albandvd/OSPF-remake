#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "lsdb.h"
#include "return.h"

int main(int argc, char *argv[]) {
    if (argc < 2) {
        fprintf(stderr, "Usage: %s <param_int>\n", argv[0]);
        return 1;
    }
    int routerID = atoi(argv[1]);
    printf("Router ID: %d\n", routerID);

    FILE *f_router = fopen("router_id.txt", "w");
    if (!f_router) {
        perror("Erreur lors de l'ouverture de router_id.txt");
        return 2;
    }
    fclose(f_router);

    // Creation des interfaces
    Interface interface1 = {
        .nameInterface = "eth0",
        .ip = "192.168.1.2",
        .mask = "255.255.255.0",
        .network = "192.168.1.0",
        .mac = "AA:BB:CC:DD:EE:FF",
    };

    Interface interface2 = {
        .nameInterface = "eth0",
        .ip = "192.168.1.2",
        .mask = "255.255.255.0",
        .network = "192.168.1.0",
        .mac = "AA:BB:CC:DD:EE:FF",
    };

    // Creation des LSAs
    LSA lsa1 = {"R1", routerID, interface1};
    LSA lsa2 = {"R2", routerID, interface2};

    // Creation de la LSDB
    LSDB lsdb = {
        .countLSA = 2,
        .lsa = {lsa1, lsa2}
    };

    printf("********* Saving LSDB *********\n");

    FILE *f = fopen("lsdb.bin", "wb");
    if (!f) {
        ReturnCode code = FILE_OPEN_ERROR;
        perror("Error opening file");
        fprintf(stderr, "[main] %s\n", return_code_to_string(code));
        return code;
    }

    size_t written = fwrite(&lsdb, sizeof(LSDB), 1, f);
    fclose(f);

    if (written != 1) {
        ReturnCode code = FILE_WRITE_ERROR;
        fprintf(stderr, "[main] %s\n", return_code_to_string(code));
        return code;
    }

    printf("LSDB initialised successfully.\n");
    return RETURN_SUCCESS;
}
