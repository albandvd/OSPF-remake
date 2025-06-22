// #include <stdio.h>
// #include <stdlib.h>
// #include <string.h>
// #include <unistd.h>
// #include <arpa/inet.h>
// #include "hello.h"

// #define PORT 4242

// int main(int argc, char *argv[])
// {
//     if (argc != 2)
//     {
//         printf("Usage : %s interface\n", argv[0]);
//         return 1;
//     }
//     int sockfd;
//     struct sockaddr_in serv_addr;

//     sockfd = socket(AF_INET, SOCK_STREAM, 0);
//     serv_addr.sin_family = AF_INET;
//     serv_addr.sin_port = htons(PORT);
//     inet_pton(AF_INET, argv[1], &serv_addr.sin_addr);

//     if (connect(sockfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0)
//     {
//         perror("Connexion échouée");
//         exit(1);
//     }

//     // Envoi du HELLO
//     HelloMessage hello;
//     hello.type = htonl(MSG_TYPE_HELLO);
//     hello.router_id = htonl(42); // notre ID
//     strncpy(hello.router_name, "RouteurA", MAX_NAME_LEN);
//     send(sockfd, &hello, sizeof(hello), 0);
//     printf("HELLO envoyé.\n");

//     // Réception du HELLO_ACK
//     HelloAckMessage ack;
//     ssize_t r = recv(sockfd, &ack, sizeof(ack), 0);
//     if (r > 0)
//     {
//         ack.type = ntohl(ack.type);
//         ack.router_id = ntohl(ack.router_id);
//         ack.status = ntohl(ack.status);
//         printf("Reçu ACK de %d, status: %d\n", ack.router_id, ack.status);
//     }
//     else
//     {
//         perror("Erreur lors de la réception de l'ACK");
//     }

//     close(sockfd);
//     return 0;
// }
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <ifaddrs.h>
#include <netinet/in.h>
#include <net/if.h>
#include <errno.h>
// #include "hello.h"
#include <jansson.h>

#define PORT 4242

void print_json()
{
    json_t *root = json_object();

    json_object_set_new(root, "nom", json_string("Bob"));
    json_object_set_new(root, "age", json_integer(45));

    // Écrit dans un fichier avec indentation (JSON pretty-print)
    if (json_dump_file(root, "test.json", JSON_INDENT(4)) != 0)
    {
        fprintf(stderr, "Erreur d'écriture JSON\n");
        return;
    }

    json_decref(root); // libère la mémoire
}

void read_json()
{
    json_error_t error;
    json_t *root = json_load_file("test.json", 0, &error);
    if (!root)
    {
        fprintf(stderr, "Erreur: %s\n", error.text);
        return;
    }

    const char *nom = json_string_value(json_object_get(root, "nom"));
    int age = json_integer_value(json_object_get(root, "age"));

    printf("Nom: %s\n", nom);
    printf("Age: %d\n", age);

    json_decref(root); // libère la mémoire
}
// void envoyer_hello(const char *ip_str)
// {
//     int sockfd;
//     struct sockaddr_in serv_addr;

//     sockfd = socket(AF_INET, SOCK_STREAM, 0);
//     if (sockfd < 0)
//         return;

//     memset(&serv_addr, 0, sizeof(serv_addr));
//     serv_addr.sin_family = AF_INET;
//     serv_addr.sin_port = htons(PORT);
//     inet_pton(AF_INET, ip_str, &serv_addr.sin_addr);

//     if (connect(sockfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0)
//     {
//         // Échec possible si aucune machine ne répond à cette IP
//         close(sockfd);
//         return;
//     }

//     HelloMessage hello;
//     hello.type = htonl(MSG_TYPE_HELLO);
//     hello.router_id = htonl(42);
//     strncpy(hello.router_name, "RouteurA", MAX_NAME_LEN);

//     send(sockfd, &hello, sizeof(hello), 0);
//     printf("HELLO envoyé à %s\n", ip_str);

//     HelloAckMessage ack;
//     ssize_t r = recv(sockfd, &ack, sizeof(ack), 0);
//     if (r > 0)
//     {
//         ack.type = ntohl(ack.type);
//         ack.router_id = ntohl(ack.router_id);
//         ack.status = ntohl(ack.status);
//         printf("ACK reçu de %s: router_id = %d, status = %d\n", ip_str, ack.router_id, ack.status);
//     }
//     else
//     {
//         perror("Réception ACK échouée");
//     }

//     close(sockfd);
//     return 0;
// }

int main(int argc, char *argv[])
{
    // if (argc != 2)
    // {
    //     fprintf(stderr, "Usage: %s <nom_interface>\n", argv[0]);
    //     return EXIT_FAILURE;
    // }

    // const char *interface = argv[1];
    // struct ifaddrs *ifaddr, *ifa;
    // char ip_str[INET_ADDRSTRLEN], netmask_str[INET_ADDRSTRLEN];

    // if (getifaddrs(&ifaddr) == -1)
    // {
    //     perror("getifaddrs");
    //     return EXIT_FAILURE;
    // }

    // uint32_t my_ip = 0, netmask = 0;

    // for (ifa = ifaddr; ifa != NULL; ifa = ifa->ifa_next)
    // {
    //     if (!ifa->ifa_addr || ifa->ifa_addr->sa_family != AF_INET)
    //         continue;

    //     if (strcmp(ifa->ifa_name, interface) == 0)
    //     {
    //         struct sockaddr_in *addr = (struct sockaddr_in *)ifa->ifa_addr;
    //         struct sockaddr_in *mask = (struct sockaddr_in *)ifa->ifa_netmask;
    //         my_ip = ntohl(addr->sin_addr.s_addr);
    //         netmask = ntohl(mask->sin_addr.s_addr);
    //         inet_ntop(AF_INET, &addr->sin_addr, ip_str, INET_ADDRSTRLEN);
    //         inet_ntop(AF_INET, &mask->sin_addr, netmask_str, INET_ADDRSTRLEN);
    //         printf("Interface %s, IP: %s, Netmask: %s\n", interface, ip_str, netmask_str);
    //         break;
    //     }
    // }

    // freeifaddrs(ifaddr);

    // if (my_ip == 0 || netmask == 0)
    // {
    //     fprintf(stderr, "Impossible de trouver l'interface %s\n", interface);
    //     return EXIT_FAILURE;
    // }

    // uint32_t network = my_ip & netmask;
    // uint32_t broadcast = network | (~netmask);

    // for (uint32_t ip = network + 1; ip < broadcast; ip++)
    // {
    //     if (ip == my_ip)
    //         continue; // ne pas s'envoyer à soi-même

    //     struct in_addr addr;
    //     addr.s_addr = htonl(ip);
    //     char target_ip[INET_ADDRSTRLEN];
    //     inet_ntop(AF_INET, &addr, target_ip, INET_ADDRSTRLEN);

    //     envoyer_hello(target_ip);
    //     printf("Envoi de paquet ...\n");
    // }
    // printf("Tous les paquets HELLO ont été envoyés. Fin du programme.\n");
    // return 0;
    print_json();
    read_json();
}
