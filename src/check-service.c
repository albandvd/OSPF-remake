#include <stdio.h>
#include "check-service.h"
#include "LSDB.h"

ServiceState checkservice() {
    FILE *file;
    if ((file = fopen("test.bin", "rb"))) {
        fclose(file);
        return SERVICE_LAUNCHED;
    } else {
        return SERVICE_NOT_LAUNCHED;
    }
}