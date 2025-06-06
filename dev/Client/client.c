// client_voisin.c
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <netdb.h>
#include <ifaddrs.h>
#include <arpa/inet.h>

#define PORT "4242"
#define MAX_LINE 100

void get_own_ip(char *ip_str)
{
    struct ifaddrs *ifaddr, *ifa;
    void *addr;
    getifaddrs(&ifaddr);
    for (ifa = ifaddr; ifa; ifa = ifa->ifa_next)
    {
        if (ifa->ifa_addr && ifa->ifa_addr->sa_family == AF_INET &&
            strcmp(ifa->ifa_name, "lo") != 0)
        {
            addr = &((struct sockaddr_in *)ifa->ifa_addr)->sin_addr;
            inet_ntop(AF_INET, addr, ip_str, INET_ADDRSTRLEN);
            break;
        }
    }
    freeifaddrs(ifaddr);
}

void envoyer_a_voisin(const char *ip, const char *mon_ip)
{
    struct addrinfo hints, *res;
    int sockfd;

    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;

    if (getaddrinfo(ip, PORT, &hints, &res) != 0)
        return;

    sockfd = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
    if (connect(sockfd, res->ai_addr, res->ai_addrlen) != -1)
    {
        send(sockfd, mon_ip, strlen(mon_ip), 0);
        printf("Envoyé à %s : %s\n", ip, mon_ip);
    }
    else
    {
        perror("Erreur de connexion à un voisin");
    }

    close(sockfd);
    freeaddrinfo(res);
}

int main(int argc, char *argv[])
{
    if (argc != 2)
    {
        printf("Usage : %s voisins.txt\n", argv[0]);
        return 1;
    }

    FILE *fp = fopen(argv[1], "r");
    if (!fp)
    {
        perror("Erreur d'ouverture du fichier");
        return 1;
    }

    char mon_ip[INET_ADDRSTRLEN];
    get_own_ip(mon_ip);
    printf("Mon IP : %s\n", mon_ip);

    char line[MAX_LINE];
    while (fgets(line, sizeof(line), fp))
    {
        line[strcspn(line, "\n")] = 0;
        envoyer_a_voisin(line, mon_ip);
    }

    fclose(fp);
    return 0;
}
