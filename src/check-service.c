#include <stdio.h>
#include "return.h"
#include "lsdb.h"

ReturnCode checkservice(void)
{
    FILE *file = fopen(JSON_FILE_NAME, "rb");

    if (!file)
    {
        fprintf(stderr, "[checkservice] %s\n", return_code_to_string(SERVICE_NOT_LAUNCHED));
        return SERVICE_NOT_LAUNCHED;
    }

    fclose(file);
    return SERVICE_LAUNCHED;
}
