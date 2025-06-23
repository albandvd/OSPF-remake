#include "lsdb.h"
#include "return.h"
#include "check-service.h"

#include <stdio.h>
#include <string.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <net/if.h>
#include <unistd.h>
#include <ifaddrs.h>
// Usage : ./prog [add|delete] <interface_name>
int main(int argc, char* argv[]) {
    printf("router id %d \n", routerID);
    if (argc >= 2 && strcmp(argv[1], "show") == 0) {
        LSDB lsdb;
        ReturnCode code = retrieve_lsdb(&lsdb);
        if (code != RETURN_SUCCESS) {
            fprintf(stderr, "Erreur lors de la récupération de la LSDB.\n");
            return code;
        }
        printf("===== LSDB =====\n");
        for (int i = 0; i < lsdb.countLSA; ++i) {
            LSA *lsa = &lsdb.lsa[i];
            printf("LSA %d : RouterName=%s, RouterID=%d\n",
                   i+1, lsa->routerName, lsa->routerID);
            printf("  Interface :\n");
            printf("    Name    : %s\n", lsa->interfaces.nameInterface);
            printf("    IP      : %s\n", lsa->interfaces.ip);
            printf("    Mask    : %s\n", lsa->interfaces.mask);
            printf("    Network : %s\n", lsa->interfaces.network);
            printf("    MAC     : %s\n", lsa->interfaces.mac);
        }
        return RETURN_SUCCESS;
    }

    if (argc < 3) {
        fprintf(stderr, "Usage: %s [add|delete] <interface_name>\n", argv[0]);
        return ERROR_INVALID_COMMAND;
    }

    const char *command = argv[1];
    const char *interface_name = argv[2];

    ReturnCode service_state = checkservice();
    if (service_state == SERVICE_NOT_LAUNCHED) {
        fprintf(stderr, "Error: service not launched.\n");
        return SERVICE_NOT_LAUNCHED;
    }

    struct ifaddrs *ifap = NULL, *ifa = NULL;
    if (getifaddrs(&ifap) != 0) {
        perror("getifaddrs failed");
        return LSDB_ERROR_READ_FAILURE;
    }

    int interface_found = 0;

    for (ifa = ifap; ifa != NULL; ifa = ifa->ifa_next) {
        if (!ifa->ifa_addr || ifa->ifa_addr->sa_family != AF_INET) {
            continue;
        }

        if (strcmp(ifa->ifa_name, interface_name) == 0) {
            interface_found = 1;

            struct sockaddr_in *sa = (struct sockaddr_in *)ifa->ifa_addr;
            struct sockaddr_in *nm = (struct sockaddr_in *)ifa->ifa_netmask;

            char ip[INET_ADDRSTRLEN] = {0};
            char netmask[INET_ADDRSTRLEN] = "N/A";
            char broadcast[INET_ADDRSTRLEN] = "N/A";
            char network[INET_ADDRSTRLEN] = "N/A";

            strncpy(ip, inet_ntoa(sa->sin_addr), INET_ADDRSTRLEN - 1);

            if (ifa->ifa_netmask) {
                strncpy(netmask, inet_ntoa(nm->sin_addr), INET_ADDRSTRLEN - 1);

                // Calcul de l'adresse réseau (IP & Netmask)
                struct in_addr net_addr;
                net_addr.s_addr = sa->sin_addr.s_addr & nm->sin_addr.s_addr;
                strncpy(network, inet_ntoa(net_addr), INET_ADDRSTRLEN - 1);
            }

            if (ifa->ifa_broadaddr) {
                struct sockaddr_in *br = (struct sockaddr_in *)ifa->ifa_broadaddr;
                strncpy(broadcast, inet_ntoa(br->sin_addr), INET_ADDRSTRLEN - 1);
            }

            // Récupération de l'adresse MAC
            char mac_address[18] = "N/A";
            int fd = socket(AF_INET, SOCK_DGRAM, 0);
            if (fd >= 0) {
                struct ifreq ifr;
                memset(&ifr, 0, sizeof(ifr));
                strncpy(ifr.ifr_name, ifa->ifa_name, IFNAMSIZ - 1);
                if (ioctl(fd, SIOCGIFHWADDR, &ifr) == 0) {
                    unsigned char *mac = (unsigned char *)ifr.ifr_hwaddr.sa_data;
                    snprintf(mac_address, sizeof(mac_address), "%02x:%02x:%02x:%02x:%02x:%02x",
                             mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
                }
                close(fd);
            }

            printf("Interface: %s\tAddress: %s\n", ifa->ifa_name, ip);

            if (strcmp(command, "add") == 0) {
                printf("Adding interface: %s\n", ifa->ifa_name);

                Interface new_interface;
                memset(&new_interface, 0, sizeof(new_interface));
                strncpy(new_interface.nameInterface, ifa->ifa_name, sizeof(new_interface.nameInterface) - 1);
                strncpy(new_interface.ip, ip, sizeof(new_interface.ip) - 1);
                strncpy(new_interface.mask, netmask, sizeof(new_interface.mask) - 1);
                strncpy(new_interface.network, network, sizeof(new_interface.network) - 1);
                strncpy(new_interface.mac, mac_address, sizeof(new_interface.mac) - 1);

                printf("  IP Address   : %s\n", ip);
                printf("  Netmask      : %s\n", netmask);
                printf("  Broadcast    : %s\n", broadcast);
                printf("  Network Addr : %s\n", network);
                printf("  MAC Address  : %s\n", mac_address);

                // Ajout réel à la LSDB
                LSA new_lsa;
                memset(&new_lsa, 0, sizeof(new_lsa));
                char hostname[3];
                gethostname(hostname, sizeof(hostname));
                if (get_routerId(&routerID) != RETURN_SUCCESS) {
                    fprintf(stderr, "Erreur lors de la récupération du Router ID.\n");
                    freeifaddrs(ifap);
                    return ROUTER_ID_NOT_FOUND;
                }

                strncpy(new_lsa.routerName, hostname, sizeof(new_lsa.routerName) - 1);
                new_lsa.routerID = routerID;
                new_lsa.interfaces = new_interface;

                LSDB lsdb;
                ReturnCode code = retrieve_lsdb(&lsdb);
                if (code != RETURN_SUCCESS) {
                    fprintf(stderr, "Erreur lors de la récupération de la LSDB.\n");
                    freeifaddrs(ifap);
                    return code;
                }
                code = add_lsa(&new_lsa, &lsdb);
                if (code != RETURN_SUCCESS) {
                    fprintf(stderr, "Erreur lors de l'ajout de la LSA à la LSDB.\n");
                    freeifaddrs(ifap);
                    return code;
                }
                printf("LSA ajoutée à la LSDB avec succès.\n");
                freeifaddrs(ifap);
                return INTERFACE_ADDED;
            } else if (strcmp(command, "delete") == 0) {
                printf("Deleting interface: %s\n", ifa->ifa_name);

                // TODO: Ajouter la logique réelle pour supprimer l'interface

                freeifaddrs(ifap);
                return INTERFACE_DELETED;
            } else {
                fprintf(stderr, "Invalid command. Use 'add' or 'delete'.\n");
                freeifaddrs(ifap);
                return ERROR_INVALID_COMMAND;
            }
        }
    }

    freeifaddrs(ifap);

    if (!interface_found) {
        fprintf(stderr, "Interface '%s' not found.\n", interface_name);
        return INTERFACE_NOT_FOUND;
    }

    return RETURN_SUCCESS;
}