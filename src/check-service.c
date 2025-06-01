#include <stdio.h>

int checkservice() {
    FILE *file;
    if (file = fopen("test.bin", "rb")){
        fclose(file);
        return 0;
    }

    return 1;
}