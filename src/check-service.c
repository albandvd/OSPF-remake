#include <stdio.h>
#include "return.h"

ReturnCode checkservice() {
    FILE *file = fopen("lsdb.json", "rb");

    if (!file) {
        fprintf(stderr, "[checkservice] %s\n", return_code_to_string(SERVICE_NOT_LAUNCHED));
        return SERVICE_NOT_LAUNCHED;
    }

    fclose(file);
    return SERVICE_LAUNCHED;
}
