#include <stdio.h>
#include "return.h"

ReturnCode checkservice() {
    FILE *file = fopen("test.bin", "rb");

    if (!file) {
        fprintf(stderr, "[checkservice] %s\n", return_code_to_string(FILE_OPEN_ERROR));
        return FILE_OPEN_ERROR;
    }

    fclose(file);
    return RETURN_SUCCESS;
}
