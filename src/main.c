#include <stdio.h>
#include <string.h>
#include "LSDB.h"

int main() {

    // Creation du LSDB
    Interface interface1 = {"eth0", "192.168.1.2", "255.255.255.0", "192.168.1.0", "AA:BB:CC:DD:EE:FF", 1};

    Interface interface2= {"eth0", "192.168.1.2", "255.255.255.0", "192.168.1.0", "AA:BB:CC:DD:EE:FF", 1};

    LSA lsa1 = {"R1", 1, interface1};
    LSA lsa2 = {"R2", 2, interface2};

    LSDB lsdb = {2, 2, {lsa1, lsa2}};

    printf("*********************************\n");
    // Creation LSDB ///////////////////////////////////

    FILE *f = fopen("test.bin", "wb");
    if (!f) {
        perror("Error opening file");
        return 1;
    }

    fwrite(&lsdb, sizeof(LSDB), 1, f);
    fclose(f);
    return 0;
}
