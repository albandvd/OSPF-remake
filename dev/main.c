
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>

int main(int argc, char *argv[]) {

    int len;

    len = strlen(argv[0]);
    printf("Argv[0] = %s, len = %d \n", argv[0], len);

    for (int i=1; i<argc; ++i) {
        len = strlen(argv[i]);
        printf("Argv[%d] = %s, len = %d \n", i, argv[i], len);
    }

}
