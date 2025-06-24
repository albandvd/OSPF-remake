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

int main(int argc, char *argv[])
{
    if (argc < 2)
    {
        fprintf(stderr, "Usage: %s <param_int>\n", argv[0]);
        return 1;
    }
    int routerID = atoi(argv[1]);
    printf("Router ID: %d\n", routerID);

    FILE *routerID_file = fopen("routerID.txt", "w");
    if (!routerID_file)
    {
        ReturnCode code = FILE_OPEN_ERROR;
        fprintf(stderr, "[main] %s\n", return_code_to_string(code));
        return code;
    }
    fprintf(routerID_file, "%d\n", routerID);
    fclose(routerID_file);

    json_t *root = json_object();
    char hostname[HOST_NAME_MAX + 1];
    gethostname(hostname, sizeof(hostname));
    json_object_set_new(root, "name", json_string(hostname));
    json_object_set_new(root, "connected", json_array());
    json_object_set_new(root, "neighbors", json_array());

    json_t *connected = json_object_get(root, "connected");
    if (!connected || !json_is_array(connected))
    {
        connected = json_array();
        json_object_set(root, "connected", connected);
    }

    printf("\n===== Interfaces réseaux détectées =====\n");
    struct ifaddrs *ifap = NULL, *ifa = NULL;
    if (getifaddrs(&ifap) != 0)
    {
        perror("getifaddrs failed");
        return 1;
    }

    for (ifa = ifap; ifa != NULL; ifa = ifa->ifa_next)
    {
        if (!ifa->ifa_addr || ifa->ifa_addr->sa_family != AF_INET)
            continue;
        if (strcmp(ifa->ifa_name, "lo") == 0)
            continue;

        struct sockaddr_in *sa = (struct sockaddr_in *)ifa->ifa_addr;
        struct sockaddr_in *nm = (struct sockaddr_in *)ifa->ifa_netmask;
        char ip[INET_ADDRSTRLEN] = {0};
        char netmask[INET_ADDRSTRLEN] = "N/A";
        char network[INET_ADDRSTRLEN] = "N/A";

        strncpy(ip, inet_ntoa(sa->sin_addr), INET_ADDRSTRLEN - 1);
        if (ifa->ifa_netmask)
        {
            strncpy(netmask, inet_ntoa(nm->sin_addr), INET_ADDRSTRLEN - 1);
            struct in_addr net_addr;
            net_addr.s_addr = sa->sin_addr.s_addr & nm->sin_addr.s_addr;
            strncpy(network, inet_ntoa(net_addr), INET_ADDRSTRLEN - 1);
        }

        // >>> Crée un objet distinct pour chaque interface
        json_t *connected_obj = json_object();
        json_object_set_new(connected_obj, "network", json_string(network));
        json_object_set_new(connected_obj, "mask", json_string(netmask));
        json_object_set_new(connected_obj, "gateway", json_string(ifa->ifa_name));
        json_object_set_new(connected_obj, "hop", json_integer(1));
        json_array_append_new(connected, connected_obj);

        printf("Interface: %s\tIP: %s\tNetmask: %s\tNetwork: %s\n", ifa->ifa_name, ip, netmask, network);
    }

    freeifaddrs(ifap);
    json_object_set(root, "connected", connected);
    if (json_dump_file(root, "lsdb.json", JSON_INDENT(4)) != 0)
    {
        fprintf(stderr, "Erreur lors de la mise à jour JSON\n");
    }

    json_decref(root);
    return RETURN_SUCCESS;
}
