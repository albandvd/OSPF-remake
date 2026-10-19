#ifndef EXCHANGES_H
#define EXCHANGES_H

#include "return.h"
#include <netinet/in.h>

ReturnCode send_json(int sockfd, const char *filename,
                     char *local_ip, size_t local_ip_len,
                     char *peer_ip,  size_t peer_ip_len);
char *receive_json(int sockfd, char *local_ip, size_t local_ip_len,
                   char *peer_ip, size_t peer_ip_len);
int set_non_blocking(int fd);
int set_blocking(int fd);
int connect_with_timeout(int sockfd, struct sockaddr_in *serv_addr, int timeout_ms);
int exchanges(const char *ip_str);
const char *find_gateway_interface_cached(const char *local_ip);

/* full_scan: sweep the whole /24 for new neighbors. When 0, only the
 * already-known neighbors on this interface are re-contacted (fast path —
 * see exchanges.c for why). */
void scan_subnet_for_ospf(const char *interface, int full_scan);
void run_server(void);

#endif
