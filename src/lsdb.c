#include "lsdb.h"
#include "return.h"
#include "check-service.h"
#include <stdio.h>
#include <stdlib.h>

ReturnCode retrieve_lsdb(LSDB *lsdb) {
    if (!lsdb) {
        fprintf(stderr, "[retrieve_lsdb] Argument LSDB NULL\n");
        return LSDB_ERROR_NULL_ARGUMENT;
    }

    ReturnCode service_status = checkservice();
    if (service_status != RETURN_SUCCESS) {
        fprintf(stderr, "[retrieve_lsdb] Service not available: %s\n", return_code_to_string(service_status));
        return LSDB_ERROR_SERVICE_NOT_AVAILABLE;
    }

    FILE *f = fopen("lsdb.bin", "rb");
    if (!f) {
        // Fichier inexistant : on initialise la LSDB à vide
        memset(lsdb, 0, sizeof(LSDB));
        lsdb->countLSA = 0;
        fprintf(stderr, "[retrieve_lsdb] lsdb.bin not found, initializing empty LSDB.\n");
        return RETURN_SUCCESS;
    }

    size_t read_size = fread(lsdb, sizeof(LSDB), 1, f);
    fclose(f);

    if (read_size != 1) {
        fprintf(stderr, "[retrieve_lsdb] Erreur de lecture du fichier lsdb.bin\n");
        memset(lsdb, 0, sizeof(LSDB));
        lsdb->countLSA = 0;
        return LSDB_ERROR_READ_FAILURE;
    }

    return RETURN_SUCCESS;
}

ReturnCode save_lsdb(LSDB *lsdb) {
    if (!lsdb) {
        ReturnCode code = LSDB_ERROR_NULL_ARGUMENT;
        fprintf(stderr, "[save_lsdb] %s\n", return_code_to_string(code));
        return code;
    }

    ReturnCode service_status = checkservice();
    if (service_status != RETURN_SUCCESS) {
        fprintf(stderr, "[save_lsdb] %s\n", return_code_to_string(service_status));
        return LSDB_ERROR_SERVICE_NOT_AVAILABLE;
    }

    FILE *f = fopen("lsdb.bin", "wb");
    if (!f) {
        ReturnCode code = FILE_OPEN_ERROR;
        perror("[save_lsdb] Error opening file");
        fprintf(stderr, "%s\n", return_code_to_string(code));
        return code;
    }

    size_t written = fwrite(lsdb, sizeof(LSDB), 1, f);
    fclose(f);

    if (written != 1) {
        ReturnCode code = FILE_WRITE_ERROR;
        fprintf(stderr, "[save_lsdb] %s\n", return_code_to_string(code));
        return code;
    }

    return RETURN_SUCCESS;
}

ReturnCode add_lsa(LSA *lsa, LSDB *lsdb) {
    if (!lsa || !lsdb) {
        ReturnCode code = LSDB_ERROR_NULL_ARGUMENT;
        fprintf(stderr, "[add_lsa] %s\n", return_code_to_string(code));
        return code;
    }

    if (lsdb->countLSA >= MAX_LSAS) {
        ReturnCode code = LSDB_ERROR_FULL_LSA;
        fprintf(stderr, "[add_lsa] %s: LSDB is full\n", return_code_to_string(code));
        return code;
    }

    lsdb->lsa[lsdb->countLSA] = *lsa;
    lsdb->countLSA++;

    ReturnCode save_status = save_lsdb(lsdb);
    if (save_status != RETURN_SUCCESS) {
        fprintf(stderr, "[add_lsa] Failed to save LSDB: %s\n", return_code_to_string(save_status));
        return save_status;
    }

    return RETURN_SUCCESS;
}

ReturnCode remove_lsa(const char *routerName, const char *nameInterface, LSDB *lsdb) {
    if (!routerName || !nameInterface || !lsdb) {
        ReturnCode code = LSDB_ERROR_NULL_ARGUMENT;
        fprintf(stderr, "[remove_lsa] %s\n", return_code_to_string(code));
        return code;
    }

    int found = 0;

    for (int i = 0; i < lsdb->countLSA; ++i) {
        if (strcmp(lsdb->lsa[i].routerName, routerName) == 0 &&
            strcmp(lsdb->lsa[i].interfaces.nameInterface, nameInterface) == 0) {

            found = 1;

            // Décaler les LSA suivantes
            for (int j = i; j < lsdb->countLSA - 1; ++j) {
                lsdb->lsa[j] = lsdb->lsa[j + 1];
            }

            lsdb->countLSA--;
            break;
        }
    }

    if (!found) {
        ReturnCode code = LSDB_ERROR_LSA_NOT_FOUND;
        fprintf(stderr, "[remove_lsa] %s: (%s, %s)\n", return_code_to_string(code), routerName, nameInterface);
        return code;
    }

    ReturnCode save_status = save_lsdb(lsdb);
    if (save_status != RETURN_SUCCESS) {
        fprintf(stderr, "[remove_lsa] Failed to save LSDB: %s\n", return_code_to_string(save_status));
        return save_status;
    }

    return RETURN_SUCCESS;
}