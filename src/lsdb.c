#include "LSDB.h"
#include <stdio.h>
#include <check-service.c>

LSDBreturn retrieve_lsdb(LSDB *lsdb) {
    ServiceState state = checkservice(); 

    switch (state) {
        case SERVICE_NOT_LAUNCHED:
            printf("Service not launched");
            return LSDB_ERROR_FILE_NOT_FOUND;
        case SERVICE_LAUNCHED:
            FILE *f = fopen("test.bin", "rb");
            if (!f) {
                perror("Error opening file");
                return LSDB_ERROR_FILE_NOT_FOUND;
            }
            
            size_t read_size = fread(&lsdb, sizeof(LSDB), 1, f);
            if (read_size != 1) {
                perror("Error reading file");
                fclose(f);
                return LSDB_ERROR_READ_FAILURE;
            }
            fclose(f);
            printf("Number of routers: %d\n", lsdb->numRouter);
            return LSDB_SUCCESS;       
    }
}

LSDBreturn save_lsdb(LSDB *lsdb) {
    ServiceState state = checkservice(); 

    switch (state) {
        case SERVICE_NOT_LAUNCHED:
            printf("Service not launched");
            return LSDB_ERROR_FILE_NOT_FOUND;

        case SERVICE_LAUNCHED:
            FILE *f = fopen("test.bin", "wb");
            if (!f) {
                perror("Error opening file");
                return LSDB_ERROR_FILE_NOT_FOUND;
            }
            
            fwrite(&lsdb, sizeof(LSDB), 1, f);
            fclose(f);
            return LSDB_SUCCESS;
    }
}

int add_lsa (LSA *lsa, LSDB *lsdb) {
    if (lsdb->numRouter >= MAX_LSAS) {
        printf("LSDB is full, cannot add more LSAs.\n");
        return -1;
    }
    
    lsdb->lsda[lsdb->countLSA] = *lsa;
    lsdb->countLSA++;

    save_lsdb(lsdb);
    return 0;
}

int remove_lsa (){
}