#include <stdio.h>
#include <string.h>
#include "LSDB.h"
#include "return.h"

int main() {
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
    LSA lsa1 = {"R1", 1, interface1};
    LSA lsa2 = {"R2", 2, interface2};

    // Creation de la LSDB
    LSDB lsdb = {
        .numRouter = 1,
        .countLSA = 2,
        .lsda = {lsa1, lsa2}
    };

    printf("********* Saving LSDB *********\n");

    FILE *f = fopen("test.bin", "wb");
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
