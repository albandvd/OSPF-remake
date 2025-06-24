#include "lsdb.h"
#include "return.h"
#include "check-service.h"
#include "exchanges.h"
#include "route.h"

#include <stdio.h>
#include <string.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <net/if.h>
#include <unistd.h>
#include <ifaddrs.h>
#include <net/if.h>

// Usage : ./prog [add|delete] <interface_name>
int main(int argc, char* argv[]) {
    if (argc < 3) {
        fprintf(stderr, "Usage: %s [add|delete] <interface_name>\n", argv[0]);
        return ERROR_INVALID_COMMAND;
    }

    const char *command = argv[1];
    const char *interface = argv[2];

    ReturnCode service_state = checkservice();
    if (service_state == SERVICE_NOT_LAUNCHED) {
        fprintf(stderr, "Error: service not launched.\n");
        return SERVICE_NOT_LAUNCHED;
    }

    if (strcmp(command, "add") == 0) {
        struct ifaddrs *ifaddr, *ifa;
        char ip_str[INET_ADDRSTRLEN], netmask_str[INET_ADDRSTRLEN];
        uint32_t my_ip = 0, netmask = 0;

        if (getifaddrs(&ifaddr) == -1)
        {
            perror("getifaddrs");
            return EXIT_FAILURE;
        }

        for (ifa = ifaddr; ifa != NULL; ifa = ifa->ifa_next)
        {
            if (!ifa->ifa_addr || ifa->ifa_addr->sa_family != AF_INET)
                continue;

            if (strcmp(ifa->ifa_name, interface) == 0)
            {
                struct sockaddr_in *addr = (struct sockaddr_in *)ifa->ifa_addr;
                struct sockaddr_in *mask = (struct sockaddr_in *)ifa->ifa_netmask;

                my_ip = ntohl(addr->sin_addr.s_addr);
                netmask = ntohl(mask->sin_addr.s_addr);

                inet_ntop(AF_INET, &addr->sin_addr, ip_str, INET_ADDRSTRLEN);
                inet_ntop(AF_INET, &mask->sin_addr, netmask_str, INET_ADDRSTRLEN);

                printf("Interface détectée : %s\n", interface);
                printf("Adresse IP locale  : %s\n", ip_str);
                printf("Masque de sous-réseau : %s\n", netmask_str);
                break;
            }
        }

        freeifaddrs(ifaddr);

        // Erreur si aucune interface n'a été trouvée.
        if (my_ip == 0 || netmask == 0)
        {
            fprintf(stderr, "Erreur : interface %s non trouvée ou sans IPv4\n", interface);
            return EXIT_FAILURE;
        }

        uint32_t network = my_ip & netmask;
        uint32_t broadcast = network | (~netmask);

        printf("Réseau détecté : %u.%u.%u.%u/24\n",
            (network >> 24) & 0xFF,
            (network >> 16) & 0xFF,
            (network >> 8) & 0xFF,
            network & 0xFF);

        for (uint32_t ip = network + 1; ip < broadcast; ip++)
        {
            if (ip == my_ip)
                continue; // éviter de s’auto-scanner

            struct in_addr addr;
            addr.s_addr = htonl(ip);
            char target_ip[INET_ADDRSTRLEN];
            inet_ntop(AF_INET, &addr, target_ip, INET_ADDRSTRLEN);

            printf("Scanning %s...\n", target_ip);
            exchanges(target_ip);

            usleep(10000); // petit délai (10 ms) pour éviter surcharge CPU
        }
        
        int sockfd,
        client_sock;
        struct sockaddr_in serv_addr, cli_addr;
        socklen_t cli_len = sizeof(cli_addr);

        // Création de la socket
        sockfd = socket(AF_INET, SOCK_STREAM, 0);
        if (sockfd < 0)
        {
            perror("socket");
            exit(EXIT_FAILURE);
        }

        // Configuration de l'adresse du serveur
        memset(&serv_addr, 0, sizeof(serv_addr));
        serv_addr.sin_family = AF_INET;
        serv_addr.sin_addr.s_addr = INADDR_ANY;
        serv_addr.sin_port = htons(PORT);

        // Bind
        if (bind(sockfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0)
        {
            perror("bind");
            close(sockfd);
            exit(EXIT_FAILURE);
        }

        // Listen
        if (listen(sockfd, 5) < 0)
        {
            perror("listen");
            close(sockfd);
            exit(EXIT_FAILURE);
        }

        printf("Serveur en attente de connexions...\n");

        while (1)
        {
            client_sock = accept(sockfd, (struct sockaddr *)&cli_addr, &cli_len);
            if (client_sock < 0)
            {
                perror("accept");
                continue;
            }

            printf("Connexion acceptée depuis %s:%d\n",
                    inet_ntoa(cli_addr.sin_addr), ntohs(cli_addr.sin_port));

            char local_ip[INET_ADDRSTRLEN];
            char peer_ip[INET_ADDRSTRLEN];

            if (send_json(client_sock, JSON_FILE_NAME, local_ip, sizeof(local_ip), peer_ip, sizeof(peer_ip)) == 0)
            {
                printf("Transmission réussie : %s -> %s\n", local_ip, peer_ip);
            }

            char *buffer = receive_json(client_sock, local_ip, sizeof(local_ip), peer_ip, sizeof(peer_ip));

            // Ouverture du JSON local
            json_error_t error;

            // Charger le JSON depuis le fichier
            json_t *root = json_load_file(JSON_FILE_NAME, 0, &error);
            if (!root)
            {
                fprintf(stderr, "Erreur chargement JSON depuis '%s' : %s\n", JSON_FILE_NAME, error.text);
                return -1;
            }
            print_json_neighbors(JSON_FILE_NAME, peer_ip, buffer);
            free(buffer);
            close(client_sock);
        }
        close(sockfd);

    } else if (strcmp(command, "delete") == 0) {
        //printf("Deleting interface: %s\n", ifa->ifa_name);

        //TODO: Ajouter la logique réelle pour supprimer l'interface

        //freeifaddrs(ifap);
        return INTERFACE_DELETED;
    } else {
        fprintf(stderr, "Invalid command. Use 'add' or 'delete'.\n");
        //freeifaddrs(ifap);
        return ERROR_INVALID_COMMAND;
    }

    return RETURN_SUCCESS;
}