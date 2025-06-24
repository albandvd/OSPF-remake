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

    // * Envoi du HELLO
    // HelloMessage hello;
    // hello.type = htonl(MSG_TYPE_HELLO);
    // hello.router_id = htonl(42);
    // char hostname[HOST_NAME_MAX + 1];
    // gethostname(hostname, sizeof(hostname));
    // strncpy(hello.router_name, hostname, MAX_NAME_LEN);
    // send(sockfd, &hello, sizeof(hello), 0);
    // printf("HELLO envoyé à %s\n", ip_str);

    // * Réception ACK
    // HelloAckMessage ack;
    // ssize_t r = recv(sockfd, &ack, sizeof(ack), 0);

    // if (r > 0)
    // {
    //     ack.type = ntohl(ack.type);
    //     ack.router_id = ntohl(ack.router_id);
    //     ack.status = ntohl(ack.status);
    //     printf("ACK reçu de %s: router_id = %d, status = %d\n", ip_str, ack.router_id, ack.status);

    //     // IP locale (interface utilisée)
    //     getsockname(sockfd, (struct sockaddr *)&local_addr, &addr_len);
    //     char local_ip[INET_ADDRSTRLEN];
    //     int hop = 1;
    //     inet_ntop(AF_INET, &local_addr.sin_addr, local_ip, sizeof(local_ip));

    //     // Destination réseau = IP cible & masque /24
    //     struct in_addr addr;
    //     inet_pton(AF_INET, ip_str, &addr);
    //     uint32_t ip_net = ntohl(addr.s_addr);
    //     ip_net &= 0xFFFFFF00; // /24
    //     addr.s_addr = htonl(ip_net);

    //     char network[INET_ADDRSTRLEN];
    //     inet_ntop(AF_INET, &addr, network, sizeof(network));

    //     Route route;
    //     strncpy(route.network, network, sizeof(route.network));
    //     strncpy(route.mask, "255.255.255.0", sizeof(route.mask));
    //     strncpy(route.gateway, ip_str, sizeof(route.gateway));
    //     route.hop = hop;
    // }
    // else
    // {
    //     perror("Réception ACK échouée");
    // }

    // * Reception JSON

    char local_ip[INET_ADDRSTRLEN];
    char peer_ip[INET_ADDRSTRLEN];

    char *buffer = receive_json(sockfd, local_ip, sizeof(local_ip), peer_ip, sizeof(peer_ip));

    // Mise à jour du JSON local
    print_json_neighbors("router_info.json", peer_ip, buffer);
    send_json(sockfd, "router_info.json", local_ip, sizeof(local_ip), peer_ip, sizeof(peer_ip));
    close(sockfd);
}

int main(int argc, char *argv[])
{
    // Regarde si un argument est passé
    if (argc != 2)
    {
        fprintf(stderr, "Usage: %s <interface>\n", argv[0]);
        return EXIT_FAILURE;
    }

    // Recherche de l'interface en paramètre

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
        envoyer_hello(target_ip);

        usleep(10000); // petit délai (10 ms) pour éviter surcharge CPU
    }

    printf("Scan terminé.\n");
    return 0;
}