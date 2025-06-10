#include "LSDB.h"
#include <stdio.h>
#include <string.h>
#include "check-service.h"
#include <arpa/inet.h>
#include <sys/socket.h>
#include <ifaddrs.h>

// enter First "Add or Delete" Seconde "Interface Name"
int interface(int argc, char* argv[]) {
    struct ifaddrs *ifap, *ifa;
    struct sockaddr_in *sa;
    //char addr_mac[INET6_ADDRSTRLEN];
    char *addr;
    ServiceState state = checkservice(); 

    switch (state) {
        case SERVICE_NOT_LAUNCHED:
            printf("Service not launched");
            return LSDB_ERROR_FILE_NOT_FOUND;
        case SERVICE_LAUNCHED:
            
            getifaddrs (&ifap);
            for (ifa = ifap; ifa; ifa = ifa->ifa_next) {
                if (ifa->ifa_addr && ifa->ifa_addr->sa_family==AF_INET && strcmp(ifa->ifa_name, argv[1]) == 0) {
                    sa = (struct sockaddr_in *) ifa->ifa_addr;
                    addr = inet_ntoa(sa->sin_addr);
                    printf("Interface: %s\tAddress: %s\n", ifa->ifa_name, addr);
                    if (strcmp(argv[1], "add") == 0) {
                        printf("Adding interface: %s\n", ifa->ifa_name);
                        Interface new_interface;
                        strncpy(new_interface.nameInterface, ifa->ifa_name, sizeof(new_interface.nameInterface));
                        strncpy(new_interface.ip, addr, sizeof(new_interface.ip));
                    } else if (strcmp(argv[1], "delete") == 0) {
                        printf("Deleting interface: %s\n", ifa->ifa_name);
                    } else {
                        printf("Invalid command. Use 'add' or 'delete'.\n");
                    }
                }
            }
            freeifaddrs(ifap);
    }
}