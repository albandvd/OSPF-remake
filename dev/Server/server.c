// serveur_voisin.c
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <netdb.h>
#include <sys/socket.h>
#include <netinet/in.h>

#define PORT "4242"
#define BUF_SIZE 100

int main()
{
    struct addrinfo hints, *res;
    int sockfd, newfd;
    struct sockaddr_storage client_addr;
    socklen_t addr_size;
    char buffer[BUF_SIZE];
    int n;

    // Identification du service
    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_INET; // IPv4
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE; // Pour bind

    getaddrinfo(NULL, PORT, &hints, &res);

    // Création du socket avec les paramètres de l'identification
    sockfd = socket(res->ai_family, res->ai_socktype, res->ai_protocol);

    // Binding du socket
    bind(sockfd, res->ai_addr, res->ai_addrlen);

    // Mise en écoute du socket
    listen(sockfd, 10);

    printf("Serveur en attente sur le port %s...\n", PORT);

    while (1)
    {
        addr_size = sizeof client_addr;
        newfd = accept(sockfd, (struct sockaddr *)&client_addr, &addr_size);

        n = recv(newfd, buffer, BUF_SIZE - 1, 0);
        if (n > 0)
        {
            buffer[n] = '\0';
            printf("Reçu de voisin : %s\n", buffer);
        }

        close(newfd);
    }

    freeaddrinfo(res);
    close(sockfd);
    return 0;
}
