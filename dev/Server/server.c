#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include "hello.h"

#define PORT 4242

void handle_client(int client_sock)
{
    HelloMessage hello;
    ssize_t r = recv(client_sock, &hello, sizeof(hello), 0);
    if (r <= 0)
    {
        perror("recv hello");
        return;
    }

    hello.type = ntohl(hello.type);
    hello.router_id = ntohl(hello.router_id);

    printf("Reçu HELLO de routeur %d (%s)\n", hello.router_id, hello.router_name);

    // Envoi de l'ACK
    HelloAckMessage ack;
    ack.type = htonl(MSG_TYPE_HELLO_ACK);
    ack.router_id = htonl(99); // notre ID (ex: 99)
    ack.status = htonl(0);     // 0 = OK

    send(client_sock, &ack, sizeof(ack), 0);
}

int main()
{
    int sockfd, client_sock;
    struct sockaddr_in serv_addr, cli_addr;
    socklen_t cli_len = sizeof(cli_addr);

    // Création de la socket
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0)
    {
        perror("socket");
        exit(EXIT_FAILURE);
    }

    // Configuration de l'adresse du serveur
    memset(&serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = INADDR_ANY;
    serv_addr.sin_port = htons(PORT);

    // Bind
    if (bind(sockfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0)
    {
        perror("bind");
        close(sockfd);
        exit(EXIT_FAILURE);
    }

    // Listen
    if (listen(sockfd, 5) < 0)
    {
        perror("listen");
        close(sockfd);
        exit(EXIT_FAILURE);
    }

    printf("Serveur en attente de connexions...\n");

    // Boucle infinie pour accepter et traiter chaque client
    while (1)
    {
        client_sock = accept(sockfd, (struct sockaddr *)&cli_addr, &cli_len);
        if (client_sock < 0)
        {
            perror("accept");
            continue; // continue au lieu de stop
        }

        printf("Connexion acceptée depuis %s:%d\n",
               inet_ntoa(cli_addr.sin_addr), ntohs(cli_addr.sin_port));

        handle_client(client_sock);
        close(client_sock); // Fermer la connexion avec ce client
    }

    // (jamais atteint ici, mais bon à avoir)
    close(sockfd);
    return 0;
}
