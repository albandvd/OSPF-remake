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

#define RECV_BUFSIZE 65536
#define CONNECT_TIMEOUT_MS 20
#define RECV_TIMEOUT_MS 100

ReturnCode send_json(int sockfd, const char *filename,
                     char *local_ip, size_t local_ip_len,
                     char *peer_ip,  size_t peer_ip_len)
{
    json_error_t error;
    json_t *root = json_load_file(filename, 0, &error);
    if (!root)
    {
        fprintf(stderr, "Erreur chargement JSON '%s': %s\n", filename, error.text);
        return JSON_LOAD_ERROR;
    }

    char *json_str = json_dumps(root, JSON_COMPACT);
    json_decref(root);
    if (!json_str)
        return JSON_DUMP_ERROR;

    struct sockaddr_in local_addr, peer_addr;
    socklen_t addr_len = sizeof(struct sockaddr_in);

    if (getsockname(sockfd, (struct sockaddr *)&local_addr, &addr_len) == 0)
        inet_ntop(AF_INET, &local_addr.sin_addr, local_ip, local_ip_len);
    else
        strncpy(local_ip, "unknown", local_ip_len);

    if (getpeername(sockfd, (struct sockaddr *)&peer_addr, &addr_len) == 0)
        inet_ntop(AF_INET, &peer_addr.sin_addr, peer_ip, peer_ip_len);
    else
        strncpy(peer_ip, "unknown", peer_ip_len);

    ssize_t total_sent = 0;
    ssize_t length = (ssize_t)strlen(json_str);
    while (total_sent < length)
    {
        ssize_t sent = send(sockfd, json_str + total_sent, length - total_sent, 0);
        if (sent <= 0)
        {
            free(json_str);
            return EXCHANGE_FAILURE;
        }
        total_sent += sent;
    }

    free(json_str);
    return RETURN_SUCCESS;
}

char *receive_json(int sockfd, char *local_ip, size_t local_ip_len,
                   char *peer_ip, size_t peer_ip_len)
{
    struct sockaddr_in local_addr, peer_addr;
    socklen_t addr_len = sizeof(struct sockaddr_in);

    if (getsockname(sockfd, (struct sockaddr *)&local_addr, &addr_len) == 0)
        inet_ntop(AF_INET, &local_addr.sin_addr, local_ip, local_ip_len);
    else
        strncpy(local_ip, "unknown", local_ip_len);

    if (getpeername(sockfd, (struct sockaddr *)&peer_addr, &addr_len) == 0)
        inet_ntop(AF_INET, &peer_addr.sin_addr, peer_ip, peer_ip_len);
    else
        strncpy(peer_ip, "unknown", peer_ip_len);

    char *buffer = malloc(RECV_BUFSIZE);
    if (!buffer)
        return NULL;

    int received = recv(sockfd, buffer, RECV_BUFSIZE - 1, 0);
    if (received <= 0)
    {
        free(buffer);
        return NULL;
    }
    buffer[received] = '\0';
    return buffer;
}

int set_non_blocking(int fd)
{
    int flags = fcntl(fd, F_GETFL, 0);
    if (flags == -1) return -1;
    return fcntl(fd, F_SETFL, flags | O_NONBLOCK);
}

int set_blocking(int fd)
{
    int flags = fcntl(fd, F_GETFL, 0);
    if (flags == -1) return -1;
    return fcntl(fd, F_SETFL, flags & ~O_NONBLOCK);
}

int connect_with_timeout(int sockfd, struct sockaddr_in *serv_addr, int timeout_ms)
{
    set_non_blocking(sockfd);

    int res = connect(sockfd, (struct sockaddr *)serv_addr, sizeof(*serv_addr));
    if (res == 0)
        return 0;

    if (errno != EINPROGRESS)
        return -1;

    fd_set wfds;
    struct timeval tv;
    FD_ZERO(&wfds);
    FD_SET(sockfd, &wfds);
    tv.tv_sec  = timeout_ms / 1000;
    tv.tv_usec = (timeout_ms % 1000) * 1000;

    res = select(sockfd + 1, NULL, &wfds, NULL, &tv);
    if (res <= 0)
        return -1;

    int so_error = 0;
    socklen_t len = sizeof(so_error);
    getsockopt(sockfd, SOL_SOCKET, SO_ERROR, &so_error, &len);
    return (so_error != 0) ? -1 : 0;
}

/* Client: connect to one IP and exchange LSDBs */
void exchanges(const char *ip_str)
{
    int sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) return;

    struct sockaddr_in serv_addr;
    memset(&serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port   = htons(PORT);
    inet_pton(AF_INET, ip_str, &serv_addr.sin_addr);

    if (connect_with_timeout(sockfd, &serv_addr, CONNECT_TIMEOUT_MS) < 0)
    {
        close(sockfd);
        return;
    }

    set_blocking(sockfd);

    struct timeval timeout;
    timeout.tv_sec  = 0;
    timeout.tv_usec = RECV_TIMEOUT_MS * 1000;
    setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));

    char local_ip[INET_ADDRSTRLEN];
    char peer_ip[INET_ADDRSTRLEN];

    /* Server sends first; we receive then reply */
    char *buffer = receive_json(sockfd, local_ip, sizeof(local_ip),
                                peer_ip, sizeof(peer_ip));
    if (buffer)
    {
        print_json_neighbors(JSON_FILE_NAME, peer_ip, buffer);
        free(buffer);
    }

    send_json(sockfd, JSON_FILE_NAME, local_ip, sizeof(local_ip),
              peer_ip, sizeof(peer_ip));
    close(sockfd);
}

/* Client: scan every host in the subnet on interface for OSPF neighbors */
void scan_subnet_for_ospf(const char *interface)
{
    struct ifaddrs *ifaddr, *ifa;
    uint32_t my_ip = 0, netmask = 0;

    if (getifaddrs(&ifaddr) == -1)
    {
        perror("getifaddrs");
        return;
    }

    for (ifa = ifaddr; ifa != NULL; ifa = ifa->ifa_next)
    {
        if (!ifa->ifa_addr || ifa->ifa_addr->sa_family != AF_INET)
            continue;
        if (strcmp(ifa->ifa_name, interface) != 0)
            continue;

        struct sockaddr_in *addr = (struct sockaddr_in *)ifa->ifa_addr;
        struct sockaddr_in *mask = (struct sockaddr_in *)ifa->ifa_netmask;
        my_ip   = ntohl(addr->sin_addr.s_addr);
        netmask = ntohl(mask->sin_addr.s_addr);
        break;
    }
    freeifaddrs(ifaddr);

    if (my_ip == 0 || netmask == 0)
        return;

    uint32_t network   = my_ip & netmask;
    uint32_t broadcast = network | (~netmask);

    for (uint32_t ip = network + 1; ip < broadcast; ip++)
    {
        if (ip == my_ip) continue;

        struct in_addr addr;
        addr.s_addr = htonl(ip);
        char target_ip[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &addr, target_ip, INET_ADDRSTRLEN);
        exchanges(target_ip);
    }
}

/* Server: listen forever, exchange LSDBs with connecting peers */
void run_server(void)
{
    int sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0)
    {
        perror("socket");
        return;
    }

    int opt = 1;
    setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    struct sockaddr_in serv_addr;
    memset(&serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family      = AF_INET;
    serv_addr.sin_addr.s_addr = INADDR_ANY;
    serv_addr.sin_port        = htons(PORT);

    if (bind(sockfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0)
    {
        perror("bind");
        close(sockfd);
        return;
    }
    if (listen(sockfd, 5) < 0)
    {
        perror("listen");
        close(sockfd);
        return;
    }

    struct sockaddr_in cli_addr;
    socklen_t cli_len = sizeof(cli_addr);

    while (1)
    {
        int client_sock = accept(sockfd, (struct sockaddr *)&cli_addr, &cli_len);
        if (client_sock < 0)
            continue;

        char local_ip[INET_ADDRSTRLEN];
        char peer_ip[INET_ADDRSTRLEN];

        /* Send our LSDB first */
        send_json(client_sock, JSON_FILE_NAME,
                  local_ip, sizeof(local_ip),
                  peer_ip,  sizeof(peer_ip));

        /* Receive peer's LSDB */
        char *buffer = receive_json(client_sock, local_ip, sizeof(local_ip),
                                    peer_ip, sizeof(peer_ip));
        if (buffer)
        {
            print_json_neighbors(JSON_FILE_NAME, peer_ip, buffer);
            free(buffer);
        }

        close(client_sock);
    }
    close(sockfd);
}
