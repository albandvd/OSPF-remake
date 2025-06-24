#ifndef EXCHANGES_H
#define EXCHANGES_H

#include "return.h"

ReturnCode send_json(int sockfd, const char *filename, char *local_ip, size_t local_ip_len, char *peer_ip, size_t peer_ip_len);
char *receive_json(int sockfd, char *local_ip, size_t local_ip_len, char *peer_ip, size_t peer_ip_len);
int set_non_blocking(int fd);
int set_blocking(int fd);
int connect_with_timeout(int sockfd, struct sockaddr_in *serv_addr, int timeout_ms);
void exchanges(const char *ip_str);

#endif