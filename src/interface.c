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
#include <sys/types.h>
#include <sys/wait.h>

// Usage : ./prog [add|delete] <interface_name>
void run_client(const char *interface)
{
    struct ifaddrs *ifaddr, *ifa;
    char ip_str[INET_ADDRSTRLEN], netmask_str[INET_ADDRSTRLEN];
    uint32_t my_ip = 0, netmask = 0;

    if (getifaddrs(&ifaddr) == -1)
    {
        perror("getifaddrs");
        exit(EXIT_FAILURE);
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
    if (my_ip == 0 || netmask == 0)
    {
        fprintf(stderr, "Erreur : interface %s non trouvée ou sans IPv4\n", interface);
        exit(EXIT_FAILURE);
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
            continue;
        struct in_addr addr;
        addr.s_addr = htonl(ip);
        char target_ip[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &addr, target_ip, INET_ADDRSTRLEN);
        printf("Scanning %s...\n", target_ip);
        exchanges(target_ip);
        usleep(10000);
    }
}

void run_server()
{
    int sockfd, client_sock;
    struct sockaddr_in serv_addr, cli_addr;
    socklen_t cli_len = sizeof(cli_addr);
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0)
    {
        perror("socket");
        exit(EXIT_FAILURE);
    }
    memset(&serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = INADDR_ANY;
    serv_addr.sin_port = htons(PORT);
    if (bind(sockfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0)
    {
        perror("bind");
        close(sockfd);
        exit(EXIT_FAILURE);
    }
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
        json_error_t error;
        json_t *root = json_load_file(JSON_FILE_NAME, 0, &error);
        if (!root)
        {
            fprintf(stderr, "Erreur chargement JSON depuis '%s' : %s\n", JSON_FILE_NAME, error.text);
            return;
        }
        print_json_neighbors(JSON_FILE_NAME, peer_ip, buffer);
        // send_json_to_ospf_neighbors(JSON_FILE_NAME);

        // Remplissage de la table de routage
        Route table[MAX_ROUTES];
        int route_count = generate_routing_table_from_file(JSON_FILE_NAME, table);

        if (route_count < 0)
        {
            fprintf(stderr, "Échec de la génération de la table de routage.\n");
            return 1;
        }

        for (int i = 0; i < route_count; ++i)
        {
            add_route(table[i].network, table[i].mask, table[i].next_hop, table[i].gateway);
            printf("Route vers %s/%s via %s (%s), hop: %d ajoutée dans la table de routage\n",
                   table[i].network,
                   table[i].mask,
                   table[i].gateway,
                   table[i].is_connected ? "connected" : table[i].next_hop,
                   table[i].hop);
        }

        free(buffer);
        close(client_sock);
    }
    close(sockfd);
}

int main(int argc, char *argv[])
{
    if (argc < 3)
    {
        fprintf(stderr, "Usage: %s [add|delete] <interface_name>\n", argv[0]);
        return ERROR_INVALID_COMMAND;
    }
    const char *command = argv[1];
    const char *interface = argv[2];
    ReturnCode service_state = checkservice();
    if (service_state == SERVICE_NOT_LAUNCHED)
    {
        fprintf(stderr, "Error: service not launched.\n");
        return SERVICE_NOT_LAUNCHED;
    }
    if (strcmp(command, "add") == 0)
    {
        set_is_ospf(JSON_FILE_NAME, interface);
        pid_t pid = fork();
        if (pid < 0)
        {
            perror("fork");
            return EXIT_FAILURE;
        }
        else if (pid == 0)
        {
            // Processus fils : serveur
            run_server();
            exit(0);
        }
        else
        {
            // Processus parent : client
            run_client(interface);
            // Optionnel : attendre la fin du serveur (sinon, le parent se termine)
            // waitpid(pid, NULL, 0);
        }
    }
    else if (strcmp(command, "delete") == 0)
    {
        // TODO: Ajouter la logique réelle pour supprimer l'interface

        return INTERFACE_DELETED;
    }
    else
    {
        fprintf(stderr, "Invalid command. Use 'add' or 'delete'.\n");
        return ERROR_INVALID_COMMAND;
    }
    return RETURN_SUCCESS;
}