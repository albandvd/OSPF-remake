#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <net/if.h>
#include <ifaddrs.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <jansson.h>
#include <limits.h>

#include "lsdb.h"
#include "return.h"

#define MAX_ROUTES 10

int main() {
    init_json(JSON_FILE_NAME);
    print_json_connected(JSON_FILE_NAME);

    return RETURN_SUCCESS;
}
