#include "route.h"
#include "lsdb.h"
#include "return.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <net/if.h>
#include <sys/ioctl.h>
#include <netinet/in.h>
#include <linux/route.h>
#include <arpa/inet.h>

ReturnCode add_route(char network[16], char mask[16], char gateway[16], char interface[IFNAMSIZ]) {
    int sockfd;
    struct rtentry route;
    struct sockaddr_in *addr;

    sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd < 0) {
        ReturnCode code = ROUTE_SOCKET_ERROR;
        fprintf(stderr, "[route] %s\n", return_code_to_string(code));
        return code;
    }

    memset(&route, 0, sizeof(route));

    addr = (struct sockaddr_in*)&route.rt_dst;
    addr->sin_family = AF_INET;
    inet_pton(AF_INET, network, &addr->sin_addr);

    addr = (struct sockaddr_in*)&route.rt_genmask;
    addr->sin_family = AF_INET;
    inet_pton(AF_INET, mask, &addr->sin_addr);

    addr = (struct sockaddr_in*)&route.rt_gateway;
    addr->sin_family = AF_INET;
    inet_pton(AF_INET, gateway, &addr->sin_addr);

    route.rt_flags = RTF_UP | RTF_GATEWAY;
    route.rt_dev = interface;

    if (ioctl(sockfd, SIOCADDRT, &route) < 0) {
        close(sockfd);
        ReturnCode code = ROUTE_IOCTL_ERROR;
        fprintf(stderr, "[route] %s\n", return_code_to_string(code));
        return code;
    }

    close(sockfd);
    return ROUTE_ADDED_SUCCESSFULLY;
}

ReturnCode delete_route(char network[16], char mask[16], char gateway[16], char interface[IFNAMSIZ]) {
    int sockfd;
    struct rtentry route;
    struct sockaddr_in *addr;

    sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd < 0) {
        ReturnCode code = ROUTE_SOCKET_ERROR;
        fprintf(stderr, "[route] %s\n", return_code_to_string(code));
        return code;
    }

    memset(&route, 0, sizeof(route));

    addr = (struct sockaddr_in*)&route.rt_dst;
    addr->sin_family = AF_INET;
    inet_pton(AF_INET, network, &addr->sin_addr);

    addr = (struct sockaddr_in*)&route.rt_genmask;
    addr->sin_family = AF_INET;
    inet_pton(AF_INET, mask, &addr->sin_addr);

    addr = (struct sockaddr_in*)&route.rt_gateway;
    addr->sin_family = AF_INET;
    inet_pton(AF_INET, gateway, &addr->sin_addr);

    route.rt_flags = RTF_UP | RTF_GATEWAY;
    route.rt_dev = interface;

    if (ioctl(sockfd, SIOCDELRT, &route) < 0) {
        close(sockfd);
        ReturnCode code = ROUTE_IOCTL_ERROR;
        fprintf(stderr, "[route] %s\n", return_code_to_string(code));
        return code;
    }

    close(sockfd);
    return ROUTE_DELETED_SUCCESSFULLY;
}