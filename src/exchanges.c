
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <ifaddrs.h>
#include <netinet/in.h>
#include <net/if.h>
#include <jansson.h>
#include <errno.h>
#include <limits.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/select.h>
#include <fcntl.h>
#include "exchanges.h"
#include "return.h"
#include "lsdb.h"

ReturnCode send_json(int sockfd, const char *filename, char *local_ip, size_t local_ip_len, char *peer_ip, size_t peer_ip_len){
    json_error_t error;

    // Charger le JSON depuis le fichier
    json_t *root = json_load_file(filename, 0, &error);
    if (!root)
    {
        fprintf(stderr, "Erreur chargement JSON depuis '%s' : %s\n", filename, error.text);
        return JSON_LOAD_ERROR;
    }

    // Convertir le JSON en chaîne compacte
    char *json_str = json_dumps(root, JSON_COMPACT);
    if (!json_str)
    {
        fprintf(stderr, "Erreur de conversion JSON en chaîne\n");
        json_decref(root);
        return JSON_DUMP_ERROR;
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
            return EXCHANGE_FAILURE;
        }
        total_sent += sent;
    }

    printf("JSON envoyé de %s à %s (%ld octets).\n", local_ip, peer_ip, total_sent);

    // Libération des ressources
    free(json_str);
    json_decref(root);
    return RETURN_SUCCESS;
}

char *receive_json(int sockfd, char *local_ip, size_t local_ip_len, char *peer_ip, size_t peer_ip_len){
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

int set_non_blocking(int fd){
    int flags = fcntl(fd, F_GETFL, 0);
    if (flags == -1)
        return -1;
    return fcntl(fd, F_SETFL, flags | O_NONBLOCK);
}

int set_blocking(int fd){
    int flags = fcntl(fd, F_GETFL, 0);
    if (flags == -1)
        return -1;
    return fcntl(fd, F_SETFL, flags & ~O_NONBLOCK);
}

int connect_with_timeout(int sockfd, struct sockaddr_in *serv_addr, int timeout_ms){
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

void exchanges (const char *ip_str){
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

    char local_ip[INET_ADDRSTRLEN];
    char peer_ip[INET_ADDRSTRLEN];

    char *buffer = receive_json(sockfd, local_ip, sizeof(local_ip), peer_ip, sizeof(peer_ip));

    print_json_neighbors(JSON_FILE_NAME, peer_ip, buffer);
    send_json(sockfd, JSON_FILE_NAME, local_ip, sizeof(local_ip), peer_ip, sizeof(peer_ip));
    close(sockfd);
}