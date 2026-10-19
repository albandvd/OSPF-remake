#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
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
#include "log.h"

#define RECV_CHUNK 65536
#define MAX_LSDB_SIZE (4 * 1024 * 1024) /* safety cap against a runaway peer */
#define CONNECT_TIMEOUT_MS 50
#define RECV_TIMEOUT_MS 500

ReturnCode send_json(int sockfd, const char *filename,
                     char *local_ip, size_t local_ip_len,
                     char *peer_ip,  size_t peer_ip_len)
{
    json_error_t error;
    json_t *root = json_load_file(filename, 0, &error);
    if (!root)
    {
        fprintf(stderr, "Error loading JSON '%s': %s\n", filename, error.text);
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
    ReturnCode result = RETURN_SUCCESS;
    while (total_sent < length)
    {
        ssize_t sent = send(sockfd, json_str + total_sent, length - total_sent, 0);
        if (sent <= 0)
        {
            result = EXCHANGE_FAILURE;
            break;
        }
        total_sent += sent;
    }

    free(json_str);

    /* Half-close our write side so the peer's recv() loop knows the
     * message is complete (see receive_json()); the socket is still open
     * for reading their reply. */
    if (result == RETURN_SUCCESS)
        shutdown(sockfd, SHUT_WR);

    return result;
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

    size_t cap = RECV_CHUNK;
    size_t len = 0;
    char *buffer = malloc(cap);
    if (!buffer)
        return NULL;

    for (;;)
    {
        if (len + 1 >= cap)
        {
            if (cap >= MAX_LSDB_SIZE)
            {
                free(buffer);
                return NULL;
            }
            size_t new_cap = cap * 2;
            if (new_cap > MAX_LSDB_SIZE)
                new_cap = MAX_LSDB_SIZE;
            char *resized = realloc(buffer, new_cap);
            if (!resized)
            {
                free(buffer);
                return NULL;
            }
            buffer = resized;
            cap = new_cap;
        }

        ssize_t received = recv(sockfd, buffer + len, cap - len - 1, 0);
        if (received < 0)
        {
            /* Receive timeout: use whatever we already have, if anything */
            if (errno == EWOULDBLOCK || errno == EAGAIN)
                break;
            free(buffer);
            return NULL;
        }
        if (received == 0)
            break; /* peer shut down its write side: message complete */

        len += (size_t)received;
    }

    if (len == 0)
    {
        free(buffer);
        return NULL;
    }

    buffer[len] = '\0';
    return buffer;
}

static int set_recv_timeout(int sockfd, int timeout_ms)
{
    struct timeval timeout;
    timeout.tv_sec  = timeout_ms / 1000;
    timeout.tv_usec = (timeout_ms % 1000) * 1000;
    return setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));
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

/* Client: connect to one IP and exchange LSDBs. Returns 1 on a successful
 * exchange, 0 if the peer could not be reached (not an OSPF neighbor, or
 * down). */
int exchanges(const char *ip_str)
{
    int sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) return 0;

    struct sockaddr_in serv_addr;
    memset(&serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port   = htons(PORT);
    inet_pton(AF_INET, ip_str, &serv_addr.sin_addr);

    if (connect_with_timeout(sockfd, &serv_addr, CONNECT_TIMEOUT_MS) < 0)
    {
        close(sockfd);
        return 0;
    }

    set_blocking(sockfd);
    set_recv_timeout(sockfd, RECV_TIMEOUT_MS);

    char local_ip[INET_ADDRSTRLEN];
    char peer_ip[INET_ADDRSTRLEN];

    /* Server sends first; we receive then reply */
    char *buffer = receive_json(sockfd, local_ip, sizeof(local_ip),
                                peer_ip, sizeof(peer_ip));
    if (!buffer)
    {
        close(sockfd);
        return 0;
    }

    const char *local_iface = find_gateway_interface_cached(local_ip);
    MergeStats stats;
    ReturnCode rc = print_json_neighbors(JSON_FILE_NAME, peer_ip, buffer, &stats);
    free(buffer);

    if (rc == RETURN_SUCCESS)
        log_event("LSDB RECEIVED", "from %s via %s (seq %d, %d entries, %d change%s)",
                   stats.peer_name, local_iface ? local_iface : "?", stats.peer_sequence,
                   stats.entry_count, stats.changes, stats.changes == 1 ? "" : "s");

    send_json(sockfd, JSON_FILE_NAME, local_ip, sizeof(local_ip),
              peer_ip, sizeof(peer_ip));
    close(sockfd);
    return 1;
}

/* Small helper so exchanges() can log which local interface a peer was
 * reached through, without having to re-parse lsdb.json just for that. */
const char *find_gateway_interface_cached(const char *local_ip)
{
    static char cached_ip[INET_ADDRSTRLEN] = "";
    static char cached_iface[IFNAMSIZ] = "";

    if (strcmp(cached_ip, local_ip) == 0)
        return cached_iface;

    json_error_t error;
    json_t *root = json_load_file(JSON_FILE_NAME, 0, &error);
    if (!root)
        return NULL;

    json_t *connected = json_object_get(root, "connected");
    const char *iface = connected ? find_gateway_interface(connected, local_ip) : NULL;
    if (iface)
    {
        strncpy(cached_ip, local_ip, sizeof(cached_ip) - 1);
        strncpy(cached_iface, iface, sizeof(cached_iface) - 1);
    }
    json_decref(root);
    return iface ? cached_iface : NULL;
}

/* ---- Known-neighbor tracking for the fast discovery path ----
 *
 * A full /24 sweep (254 connection attempts at CONNECT_TIMEOUT_MS each)
 * takes seconds per interface and dominates failure-detection time. Most
 * cycles don't need it: once a neighbor has been seen on an interface, we
 * only need to re-contact that one IP to know if it is still there. A full
 * sweep still runs periodically (FULL_SCAN_EVERY_N_CYCLES) to discover new
 * neighbors or interfaces that just came up.
 */
#define MAX_KNOWN_PER_IFACE 32
/* Require this many consecutive failed exchanges before dropping a known
 * neighbor. A single timeout under host/CPU scheduling jitter (several
 * routers forking+exec'ing concurrently) is not reliable evidence that a
 * neighbor is actually down, and treating it as such caused visible route
 * flapping between two equally-valid equal-cost paths in testing. */
#define NEIGHBOR_DOWN_THRESHOLD 2

typedef struct
{
    char ip[INET_ADDRSTRLEN];
    int consecutive_failures;
} KnownNeighbor;

typedef struct
{
    char iface[IFNAMSIZ];
    KnownNeighbor entries[MAX_KNOWN_PER_IFACE];
    int count;
} KnownNeighbors;

static KnownNeighbors g_known[MAX_IFACE];
static int g_known_iface_count = 0;

static KnownNeighbors *known_slot_for(const char *interface)
{
    for (int i = 0; i < g_known_iface_count; i++)
        if (strcmp(g_known[i].iface, interface) == 0)
            return &g_known[i];

    if (g_known_iface_count >= MAX_IFACE)
        return NULL;

    KnownNeighbors *slot = &g_known[g_known_iface_count++];
    strncpy(slot->iface, interface, IFNAMSIZ - 1);
    slot->iface[IFNAMSIZ - 1] = '\0';
    slot->count = 0;
    return slot;
}

static KnownNeighbor *known_find(KnownNeighbors *slot, const char *ip)
{
    for (int i = 0; i < slot->count; i++)
        if (strcmp(slot->entries[i].ip, ip) == 0)
            return &slot->entries[i];
    return NULL;
}

static int known_contains(KnownNeighbors *slot, const char *ip)
{
    return known_find(slot, ip) != NULL;
}

static void known_add(KnownNeighbors *slot, const char *ip)
{
    KnownNeighbor *existing = known_find(slot, ip);
    if (existing)
    {
        existing->consecutive_failures = 0;
        return;
    }
    if (slot->count >= MAX_KNOWN_PER_IFACE)
        return;
    strncpy(slot->entries[slot->count].ip, ip, INET_ADDRSTRLEN - 1);
    slot->entries[slot->count].ip[INET_ADDRSTRLEN - 1] = '\0';
    slot->entries[slot->count].consecutive_failures = 0;
    slot->count++;
}

/* Returns 1 if the neighbor was dropped (threshold reached), 0 if it is
 * being given another chance. */
static int known_fail(KnownNeighbors *slot, const char *ip)
{
    for (int i = 0; i < slot->count; i++)
    {
        if (strcmp(slot->entries[i].ip, ip) != 0)
            continue;

        slot->entries[i].consecutive_failures++;
        if (slot->entries[i].consecutive_failures < NEIGHBOR_DOWN_THRESHOLD)
            return 0;

        slot->entries[i] = slot->entries[slot->count - 1];
        slot->count--;
        return 1;
    }
    return 0;
}

static void scan_full_subnet(const char *interface, KnownNeighbors *slot)
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

        log_debug("probing %s on %s", target_ip, interface);
        if (exchanges(target_ip))
        {
            if (!known_contains(slot, target_ip))
                log_event("NEIGHBOR UP", "%s on %s", target_ip, interface);
            known_add(slot, target_ip);
        }
    }
}

void scan_subnet_for_ospf(const char *interface, int full_scan)
{
    KnownNeighbors *slot = known_slot_for(interface);
    if (!slot)
        return;

    if (full_scan || slot->count == 0)
    {
        scan_full_subnet(interface, slot);
        return;
    }

    /* Fast path: only re-contact neighbors we already know about */
    for (int i = slot->count - 1; i >= 0; i--)
    {
        char ip[INET_ADDRSTRLEN];
        strncpy(ip, slot->entries[i].ip, sizeof(ip) - 1);
        ip[sizeof(ip) - 1] = '\0';

        if (exchanges(ip))
        {
            KnownNeighbor *n = known_find(slot, ip);
            if (n) n->consecutive_failures = 0;
        }
        else if (known_fail(slot, ip))
        {
            log_event("NEIGHBOR DOWN", "%s on %s", ip, interface);
        }
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

    log_event("SERVER LISTENING", "TCP %d", PORT);

    struct sockaddr_in cli_addr;
    socklen_t cli_len = sizeof(cli_addr);

    while (1)
    {
        int client_sock = accept(sockfd, (struct sockaddr *)&cli_addr, &cli_len);
        if (client_sock < 0)
            continue;

        set_recv_timeout(client_sock, RECV_TIMEOUT_MS);

        char local_ip[INET_ADDRSTRLEN];
        char peer_ip[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &cli_addr.sin_addr, peer_ip, sizeof(peer_ip));

        /* Figure out which local interface this connection came in on, and
         * silently drop it if that interface isn't enabled for OSPF — the
         * listening socket itself is INADDR_ANY, so this is the one place
         * that enforces "interface delete" (cahier des charges 1.4). */
        struct sockaddr_in local_addr;
        socklen_t local_len = sizeof(local_addr);
        char incoming_local_ip[INET_ADDRSTRLEN] = "";
        if (getsockname(client_sock, (struct sockaddr *)&local_addr, &local_len) == 0)
            inet_ntop(AF_INET, &local_addr.sin_addr, incoming_local_ip, sizeof(incoming_local_ip));

        const char *incoming_iface = find_gateway_interface_cached(incoming_local_ip);
        if (!incoming_iface || !is_interface_ospf_enabled(JSON_FILE_NAME, incoming_iface))
        {
            close(client_sock);
            continue;
        }

        /* Send our LSDB first */
        send_json(client_sock, JSON_FILE_NAME,
                  local_ip, sizeof(local_ip),
                  peer_ip,  sizeof(peer_ip));

        /* Receive peer's LSDB */
        char *buffer = receive_json(client_sock, local_ip, sizeof(local_ip),
                                    peer_ip, sizeof(peer_ip));
        if (buffer)
        {
            MergeStats stats;
            ReturnCode rc = print_json_neighbors(JSON_FILE_NAME, peer_ip, buffer, &stats);
            if (rc == RETURN_SUCCESS)
                log_event("LSDB RECEIVED", "from %s via %s (seq %d, %d entries, %d change%s)",
                           stats.peer_name, incoming_iface, stats.peer_sequence,
                           stats.entry_count, stats.changes, stats.changes == 1 ? "" : "s");
            free(buffer);
        }

        close(client_sock);
    }
    close(sockfd);
}
