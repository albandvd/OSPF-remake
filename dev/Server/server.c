#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <jansson.h>
#include <ifaddrs.h>
#include "hello.h"
#include <limits.h>

#define PORT 4242

typedef struct
{
    char network[20];
    char mask[20];
    char gateway[20];
    int hop;
    char next_hop[20];
} Route;

int init_json(const char *output_file)
{
    json_error_t error;
    json_t *root = json_object();
    if (!root)
    {
        fprintf(stderr, "Erreur lors de la création de l'objet JSON\n");
        return 1;
    }

    // Obtenir le nom d'hôte de la machine
    char hostname[HOST_NAME_MAX + 1];
    if (gethostname(hostname, sizeof(hostname)) != 0)
    {
        perror("gethostname");
        json_decref(root);
        return 1;
    }

    // Remplir les champs de base
    json_object_set_new(root, "name", json_string(hostname));
    json_object_set_new(root, "connected", json_array());
    json_object_set_new(root, "neighbors", json_array());

    // Écrire dans le fichier
    if (json_dump_file(root, output_file, JSON_INDENT(4)) != 0)
    {
        fprintf(stderr, "Erreur lors de l'écriture dans le fichier JSON '%s'\n", output_file);
        json_decref(root);
        return 1;
    }

    // Nettoyage
    json_decref(root);
    return 0;
}

int print_json_connected(const char *output_file)
{
    json_error_t error;
    json_t *root = json_load_file(output_file, 0, &error);
    if (!root)
    {
        fprintf(stderr, "Erreur lors de l'ouverture du fichier JSON '%s' : %s (ligne %d, colonne %d)\n",
                output_file, error.text, error.line, error.column);
        return 1;
    }
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
    if (json_dump_file(root, "router_info.json", JSON_INDENT(4)) != 0)
    {
        fprintf(stderr, "Erreur lors de la mise à jour JSON\n");
    }

    json_decref(root);
    return 0;
}

// Utilitaire : vérifie si un réseau existe déjà dans une liste json
int route_exists(json_t *array, const char *network, const char *mask)
{
    size_t i;
    json_t *elem;
    json_array_foreach(array, i, elem)
    {
        const char *net = json_string_value(json_object_get(elem, "network"));
        const char *msk = json_string_value(json_object_get(elem, "mask"));
        if (net && msk && strcmp(net, network) == 0 && strcmp(msk, mask) == 0)
            return 1;
    }
    return 0;
}

// Trouve l'interface de R1 (gateway) utilisée pour atteindre une IP (next_hop)
const char *find_gateway_interface(json_t *connected, const char *ip)
{
    size_t i;
    json_t *entry;
    struct in_addr addr_ip, addr_net, addr_mask, addr_calc;

    if (!inet_aton(ip, &addr_ip))
        return NULL;

    json_array_foreach(connected, i, entry)
    {
        const char *network = json_string_value(json_object_get(entry, "network"));
        const char *mask = json_string_value(json_object_get(entry, "mask"));
        const char *gateway = json_string_value(json_object_get(entry, "gateway"));
        if (!network || !mask || !gateway)
            continue;

        inet_aton(network, &addr_net);
        inet_aton(mask, &addr_mask);

        addr_calc.s_addr = addr_ip.s_addr & addr_mask.s_addr;
        if (addr_calc.s_addr == addr_net.s_addr)
            return gateway;
    }

    return NULL;
}

void print_json_neighbors(const char *my_json_file, const char *peer_ip, const char *peer_json_text)
{
    json_error_t error;
    json_t *my_root = json_load_file(my_json_file, 0, &error);
    json_t *peer_root = json_loads(peer_json_text, 0, &error);

    if (!my_root || !peer_root)
    {
        fprintf(stderr, "Erreur lors du chargement JSON : %s\n", error.text);
        return;
    }

    json_t *my_connected = json_object_get(my_root, "connected");
    json_t *my_neighbors = json_object_get(my_root, "neighbors");
    if (!json_is_array(my_connected) || !json_is_array(my_neighbors))
    {
        fprintf(stderr, "Champs 'connected' ou 'neighbors' manquants ou invalides.\n");
        return;
    }

    json_t *peer_connected = json_object_get(peer_root, "connected");
    if (!json_is_array(peer_connected))
        return;

    size_t i;
    json_t *peer_route;
    json_array_foreach(peer_connected, i, peer_route)
    {
        const char *network = json_string_value(json_object_get(peer_route, "network"));
        const char *mask = json_string_value(json_object_get(peer_route, "mask"));
        if (!network || !mask)
            continue;

        if (route_exists(my_connected, network, mask))
            continue;
        if (route_exists(my_neighbors, network, mask))
            continue;

        // Ajouter aux neighbors
        const char *gateway = find_gateway_interface(my_connected, peer_ip);
        if (!gateway)
            continue;

        json_t *neighbor = json_object();
        json_object_set_new(neighbor, "network", json_string(network));
        json_object_set_new(neighbor, "mask", json_string(mask));
        json_object_set_new(neighbor, "gateway", json_string(gateway));
        json_object_set_new(neighbor, "hop", json_integer(2));
        json_object_set_new(neighbor, "next_hop", json_string(peer_ip));
        json_array_append_new(my_neighbors, neighbor);
    }

    json_dump_file(my_root, my_json_file, JSON_INDENT(4));
    json_decref(my_root);
    json_decref(peer_root);
}

/**
 * Envoie un fichier JSON via une socket TCP et récupère les adresses IP locale et distante.
 *
 * @param sockfd         Descripteur de la socket connectée
 * @param filename       Chemin du fichier JSON à envoyer
 * @param local_ip       Buffer pour stocker l'adresse IP locale
 * @param local_ip_len   Taille du buffer local_ip
 * @param peer_ip        Buffer pour stocker l'adresse IP distante
 * @param peer_ip_len    Taille du buffer peer_ip
 *
 * @return 0 en cas de succès, -1 sinon
 */
int send_json(int sockfd, const char *filename, char *local_ip, size_t local_ip_len, char *peer_ip, size_t peer_ip_len)
{
    json_error_t error;

    // Charger le JSON depuis le fichier
    json_t *root = json_load_file(filename, 0, &error);
    if (!root)
    {
        fprintf(stderr, "Erreur chargement JSON depuis '%s' : %s\n", filename, error.text);
        return -1;
    }

    // Convertir le JSON en chaîne compacte
    char *json_str = json_dumps(root, JSON_COMPACT);
    if (!json_str)
    {
        fprintf(stderr, "Erreur de conversion JSON en chaîne\n");
        json_decref(root);
        return -1;
    }

    // Récupération des adresses IP
    struct sockaddr_in local_addr, peer_addr;
    socklen_t addr_len = sizeof(struct sockaddr_in);

    // Adresse locale (source)
    if (getsockname(sockfd, (struct sockaddr *)&local_addr, &addr_len) == 0)
    {
        inet_ntop(AF_INET, &local_addr.sin_addr, local_ip, local_ip_len);
    }
    else
    {
        perror("getsockname");
        strncpy(local_ip, "unknown", local_ip_len);
    }

    // Adresse distante (peer)
    if (getpeername(sockfd, (struct sockaddr *)&peer_addr, &addr_len) == 0)
    {
        inet_ntop(AF_INET, &peer_addr.sin_addr, peer_ip, peer_ip_len);
    }
    else
    {
        perror("getpeername");
        strncpy(peer_ip, "unknown", peer_ip_len);
    }

    // Envoi de la chaîne JSON
    ssize_t total_sent = 0;
    ssize_t length = strlen(json_str);
    while (total_sent < length)
    {
        ssize_t sent = send(sockfd, json_str + total_sent, length - total_sent, 0);
        if (sent <= 0)
        {
            perror("send");
            free(json_str);
            json_decref(root);
            return -1;
        }
        total_sent += sent;
    }

    printf("JSON envoyé de %s à %s (%ld octets).\n", local_ip, peer_ip, total_sent);

    // Libération des ressources
    free(json_str);
    json_decref(root);
    return 0;
}

// void handle_client(int client_sock)
// {
//     HelloMessage hello;
//     ssize_t r = recv(client_sock, &hello, sizeof(hello), 0);
//     if (r <= 0)
//     {
//         perror("recv hello");
//         return;
//     }

//     hello.type = ntohl(hello.type);
//     hello.router_id = ntohl(hello.router_id);

//     printf("Reçu HELLO de routeur %d (%s)\n", hello.router_id, hello.router_name);

//     // Envoi de l'ACK
//     HelloAckMessage ack;
//     ack.type = htonl(MSG_TYPE_HELLO_ACK);
//     ack.router_id = htonl(99); // notre ID (ex: 99)
//     ack.status = htonl(0);     // 0 = OK

//     send(client_sock, &ack, sizeof(ack), 0);
// }

char *receive_json(int sockfd, char *local_ip, size_t local_ip_len, char *peer_ip, size_t peer_ip_len)
{
    // Récupération des adresses IP
    struct sockaddr_in local_addr, peer_addr;
    socklen_t addr_len = sizeof(struct sockaddr_in);

    // Adresse locale (source)
    if (getsockname(sockfd, (struct sockaddr *)&local_addr, &addr_len) == 0)
    {
        inet_ntop(AF_INET, &local_addr.sin_addr, local_ip, local_ip_len);
    }
    else
    {
        perror("getsockname");
        strncpy(local_ip, "unknown", local_ip_len);
    }

    // Adresse distante (peer)
    if (getpeername(sockfd, (struct sockaddr *)&peer_addr, &addr_len) == 0)
    {
        inet_ntop(AF_INET, &peer_addr.sin_addr, peer_ip, peer_ip_len);
    }
    else
    {
        perror("getpeername");
        strncpy(peer_ip, "unknown", peer_ip_len);
    }

    char *buffer = malloc(8192);
    if (!buffer)
        return NULL;
    int received = recv(sockfd, buffer, 8192 - 1, 0);
    if (received < 0)
    {
        perror("recv");
        close(sockfd);
        exit(EXIT_FAILURE);
    }
    buffer[received] = '\0';
    printf("JSON received from %s", peer_ip);
    return buffer;
}

int main()
{
    init_json("router_info.json");
    print_json_connected("router_info.json");
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

        if (send_json(client_sock, "router_info.json", local_ip, sizeof(local_ip), peer_ip, sizeof(peer_ip)) == 0)
        {
            printf("Transmission réussie : %s -> %s\n", local_ip, peer_ip);
        }

        char *buffer = receive_json(client_sock, local_ip, sizeof(local_ip), peer_ip, sizeof(peer_ip));

        // Ouverture du JSON local
        json_error_t error;

        // Charger le JSON depuis le fichier
        json_t *root = json_load_file("router_info.json", 0, &error);
        if (!root)
        {
            fprintf(stderr, "Erreur chargement JSON depuis '%s' : %s\n", "router_info.json", error.text);
            return -1;
        }
        print_json_neighbors("router_info.json", peer_ip, buffer);
        free(buffer);
        close(client_sock);
    }

    close(sockfd);
    return 0;
}
