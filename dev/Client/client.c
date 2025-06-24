#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <ifaddrs.h>
#include <netinet/in.h>
#include <net/if.h>
#include <errno.h>
#include "hello.h"
#include <jansson.h>
#include <netdb.h>
#include <sys/time.h>
#include <limits.h>
#include <fcntl.h>
#include <sys/select.h>

#define __USE_POSIX

#define PORT 4242
#define MAX_NAME_LEN 50
#define MAX_ROUTES 10

typedef struct
{
    char network[20];
    char mask[20];
    char gateway[20];
    int hop;
    char next_hop[20];
} Route;

int init_json(const char *output_file, Route *new_route)
{
    json_error_t error;
    json_t *root = json_load_file(output_file, 0, &error);
    if (!root)
    {
        // Création d'un nouveau JSON si fichier inexistant
        root = json_object();
        char hostname[HOST_NAME_MAX + 1];
        gethostname(hostname, sizeof(hostname));
        json_object_set_new(root, "name", json_string(hostname));
        json_object_set_new(root, "connected", json_array());
        json_object_set_new(root, "neighbors", json_array());
    }

    json_t *connected = json_object_get(root, "connected");
    if (!connected || !json_is_array(connected))
    {
        connected = json_array();
        json_object_set(root, "connected", connected);
    }

    // Création du nouvel objet route
    json_t *connected_obj = json_object();
    json_object_set_new(connected_obj, "network", json_string(new_route->network));
    json_object_set_new(connected_obj, "mask", json_string(new_route->mask));
    json_object_set_new(connected_obj, "gateway", json_string(new_route->gateway));
    json_object_set_new(connected_obj, "hop", json_integer(new_route->hop));

    // Recherche d'une entrée avec le même "network" et "mask"
    size_t index;
    json_t *value;
    json_array_foreach(connected, index, value)
    {
        const char *existing_network = json_string_value(json_object_get(value, "network"));
        const char *existing_mask = json_string_value(json_object_get(value, "mask"));

        if (existing_network && existing_mask &&
            strcmp(existing_network, new_route->network) == 0 &&
            strcmp(existing_mask, new_route->mask) == 0)
        {
            // Remplace l'entrée existante
            json_array_set(connected, index, connected_obj);
            goto save_and_exit;
        }
    }

    // Si aucune correspondance, ajoute une nouvelle entrée
    json_array_append_new(connected, connected_obj);

save_and_exit:
    json_object_set(root, "connected", connected);

    if (json_dump_file(root, output_file, JSON_INDENT(4)) != 0)
    {
        fprintf(stderr, "Erreur lors de la mise à jour JSON\n");
    }

    json_decref(root);
    return 0;
}

void print_json_append(const char *output_file, Route *new_route)
{
    json_error_t error;
    json_t *root = json_load_file(output_file, 0, &error);
    if (!root)
    {
        fprintf(stderr, "Erreur lors de l'ouverture du fichier JSON '%s' : %s (ligne %d, colonne %d)\n",
                output_file, error.text, error.line, error.column);
        return;
    }

    json_t *routes = json_object_get(root, "routes");
    if (!routes || !json_is_array(routes))
    {
        routes = json_array();
        json_object_set(root, "routes", routes);
    }

    // Création de l'objet route
    json_t *route_obj = json_object();
    json_object_set_new(route_obj, "network", json_string(new_route->network));
    json_object_set_new(route_obj, "mask", json_string(new_route->mask));
    json_object_set_new(route_obj, "gateway", json_string(new_route->gateway));
    json_object_set_new(route_obj, "hop", json_integer(new_route->hop));
    json_object_set_new(route_obj, "next_hop", json_string(new_route->next_hop));

    // Vérifie si une route identique existe déjà (network + mask + gateway)
    size_t index;
    json_t *value;
    json_array_foreach(routes, index, value)
    {
        const char *existing_network = json_string_value(json_object_get(value, "network"));
        const char *existing_mask = json_string_value(json_object_get(value, "mask"));
        const char *existing_gateway = json_string_value(json_object_get(value, "gateway"));

        if (existing_network && existing_mask && existing_gateway &&
            strcmp(existing_network, new_route->network) == 0 &&
            strcmp(existing_mask, new_route->mask) == 0 &&
            strcmp(existing_gateway, new_route->gateway) == 0)
        {
            // Met à jour l'entrée existante
            json_array_set(routes, index, route_obj);
            goto save_and_exit;
        }
    }

    // Si aucune entrée ne correspond, on ajoute
    json_array_append_new(routes, route_obj);

save_and_exit:
    if (json_dump_file(root, output_file, JSON_INDENT(4)) != 0)
    {
        fprintf(stderr, "Erreur lors de la mise à jour JSON\n");
    }

    json_decref(root);
}

void read_json()
{
    json_error_t error;
    json_t *root = json_load_file("router-info.json", 0, &error);
    if (!root)
    {
        fprintf(stderr, "Erreur: %s\n", error.text);
        return;
    }

    const char *nom = json_string_value(json_object_get(root, "name"));
    int age = json_integer_value(json_object_get(root, "age"));

    printf("Name: %s\n", nom);
    printf("Age: %d\n", age);

    json_decref(root); // libère la mémoire
}

int set_non_blocking(int fd)
{
    int flags = fcntl(fd, F_GETFL, 0);
    if (flags == -1)
        return -1;
    return fcntl(fd, F_SETFL, flags | O_NONBLOCK);
}

int set_blocking(int fd)
{
    int flags = fcntl(fd, F_GETFL, 0);
    if (flags == -1)
        return -1;
    return fcntl(fd, F_SETFL, flags & ~O_NONBLOCK);
}

int connect_with_timeout(int sockfd, struct sockaddr_in *serv_addr, int timeout_ms)
{
    set_non_blocking(sockfd); // OBLIGATOIRE

    int res = connect(sockfd, (struct sockaddr *)serv_addr, sizeof(*serv_addr));
    if (res == 0)
        return 0; // Connexion immédiate

    if (errno != EINPROGRESS)
        return -1;

    fd_set wfds;
    struct timeval tv;
    FD_ZERO(&wfds);
    FD_SET(sockfd, &wfds);
    tv.tv_sec = timeout_ms / 1000;
    tv.tv_usec = (timeout_ms % 1000) * 1000;

    res = select(sockfd + 1, NULL, &wfds, NULL, &tv);
    if (res <= 0)
        return -1; // timeout ou erreur

    int so_error = 0;
    socklen_t len = sizeof(so_error);
    getsockopt(sockfd, SOL_SOCKET, SO_ERROR, &so_error, &len);
    if (so_error != 0)
        return -1;

    return 0; // Succès
}

void envoyer_hello(const char *ip_str)
{
    int sockfd;
    struct sockaddr_in serv_addr, local_addr;
    socklen_t addr_len = sizeof(local_addr);

    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0)
        return;

    memset(&serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(PORT);
    inet_pton(AF_INET, ip_str, &serv_addr.sin_addr);

    // Connexion avec timeout de 50ms
    if (connect_with_timeout(sockfd, &serv_addr, 50) < 0)
    {
        close(sockfd);
        return;
    }

    // Repasser le socket en bloquant pour recv()
    set_blocking(sockfd);

    // Timeout pour recv() (100 ms)
    struct timeval timeout;
    timeout.tv_sec = 0;
    timeout.tv_usec = 100000;
    setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));

    // Envoi du HELLO
    HelloMessage hello;
    hello.type = htonl(MSG_TYPE_HELLO);
    hello.router_id = htonl(42);
    char hostname[HOST_NAME_MAX + 1];
    gethostname(hostname, sizeof(hostname));
    strncpy(hello.router_name, hostname, MAX_NAME_LEN);
    send(sockfd, &hello, sizeof(hello), 0);
    printf("HELLO envoyé à %s\n", ip_str);

    // Réception ACK
    HelloAckMessage ack;
    ssize_t r = recv(sockfd, &ack, sizeof(ack), 0);

    //! ---Temporaire---

    // Initialisation des voisins directs du routeur (init_ospf)

    Route new_route1;
    strncpy(new_route1.network, "172.17.0.0", sizeof(new_route1.network));
    strncpy(new_route1.mask, "255.255.0.0", sizeof(new_route1.mask));
    strncpy(new_route1.gateway, "eth0", sizeof(new_route1.gateway));
    new_route1.hop = 1;
    init_json("router_info.json", &new_route1);

    Route new_route2;
    strncpy(new_route2.network, "172.17.1.0", sizeof(new_route2.network));
    strncpy(new_route2.mask, "255.255.0.0", sizeof(new_route2.mask));
    strncpy(new_route2.gateway, "eth1", sizeof(new_route2.gateway));
    new_route2.hop = 1;
    init_json("router_info.json", &new_route2);

    strncpy(new_route1.network, "172.17.0.0", sizeof(new_route1.network));
    strncpy(new_route1.mask, "255.255.0.0", sizeof(new_route1.mask));
    strncpy(new_route1.gateway, "eth21", sizeof(new_route1.gateway));
    new_route1.hop = 1;
    init_json("router_info.json", &new_route1);

    //! ---Fin temporaire---
    if (r > 0)
    {
        ack.type = ntohl(ack.type);
        ack.router_id = ntohl(ack.router_id);
        ack.status = ntohl(ack.status);
        printf("ACK reçu de %s: router_id = %d, status = %d\n", ip_str, ack.router_id, ack.status);

        // IP locale (interface utilisée)
        getsockname(sockfd, (struct sockaddr *)&local_addr, &addr_len);
        char local_ip[INET_ADDRSTRLEN];
        int hop = 1;
        inet_ntop(AF_INET, &local_addr.sin_addr, local_ip, sizeof(local_ip));

        // Destination réseau = IP cible & masque /24
        struct in_addr addr;
        inet_pton(AF_INET, ip_str, &addr);
        uint32_t ip_net = ntohl(addr.s_addr);
        ip_net &= 0xFFFFFF00; // /24
        addr.s_addr = htonl(ip_net);

        char network[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &addr, network, sizeof(network));

        Route route;
        strncpy(route.network, network, sizeof(route.network));
        strncpy(route.mask, "255.255.255.0", sizeof(route.mask));
        strncpy(route.gateway, ip_str, sizeof(route.gateway));
        route.hop = hop;

        // print_json_append("router_info.json", &route);
    }
    else
    {
        perror("Réception ACK échouée");
    }

    close(sockfd);
}

int main(int argc, char *argv[])
{
    if (argc != 2)
    {
        fprintf(stderr, "Usage: %s <interface>\n", argv[0]);
        return EXIT_FAILURE;
    }

    const char *interface = argv[1];
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
            netmask = 0xFFFFFF00;

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
        envoyer_hello(target_ip);

        usleep(10000); // petit délai (10 ms) pour éviter surcharge CPU
    }

    printf("Scan terminé.\n");
    return 0;
}