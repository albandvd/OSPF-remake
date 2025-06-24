#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <jansson.h>
#include "hello.h"
#include <limits.h>

#define PORT 4242

typedef struct
{
    char network[20];
    char mask[20];
    char gateway[20];
    int hop;
    char next_hop[20];
} Route;

int init_json(const char *output_file, Route *new_route)
{
    json_error_t error;
    json_t *root = json_load_file(output_file, 0, &error);
    if (!root)
    {
        // Création d'un nouveau JSON si fichier inexistant
        root = json_object();
        char hostname[HOST_NAME_MAX + 1];
        gethostname(hostname, sizeof(hostname));
        json_object_set_new(root, "name", json_string(hostname));
        json_object_set_new(root, "connected", json_array());
        json_object_set_new(root, "neighbors", json_array());
    }

    json_t *connected = json_object_get(root, "connected");
    if (!connected || !json_is_array(connected))
    {
        connected = json_array();
        json_object_set(root, "connected", connected);
    }

    // Création du nouvel objet route
    json_t *connected_obj = json_object();
    json_object_set_new(connected_obj, "network", json_string(new_route->network));
    json_object_set_new(connected_obj, "mask", json_string(new_route->mask));
    json_object_set_new(connected_obj, "gateway", json_string(new_route->gateway));
    json_object_set_new(connected_obj, "hop", json_integer(new_route->hop));

    // Recherche d'une entrée avec le même "network" et "mask"
    size_t index;
    json_t *value;
    json_array_foreach(connected, index, value)
    {
        const char *existing_network = json_string_value(json_object_get(value, "network"));
        const char *existing_mask = json_string_value(json_object_get(value, "mask"));

        if (existing_network && existing_mask &&
            strcmp(existing_network, new_route->network) == 0 &&
            strcmp(existing_mask, new_route->mask) == 0)
        {
            // Remplace l'entrée existante
            json_array_set(connected, index, connected_obj);
            goto save_and_exit;
        }
    }

    // Si aucune correspondance, ajoute une nouvelle entrée
    json_array_append_new(connected, connected_obj);

save_and_exit:
    json_object_set(root, "connected", connected);

    if (json_dump_file(root, output_file, JSON_INDENT(4)) != 0)
    {
        fprintf(stderr, "Erreur lors de la mise à jour JSON\n");
    }

    json_decref(root);
    return 0;
}

void print_json_append(const char *output_file, Route *new_route)
{
    json_error_t error;
    json_t *root = json_load_file(output_file, 0, &error);
    if (!root)
    {
        fprintf(stderr, "Erreur lors de l'ouverture du fichier JSON '%s' : %s (ligne %d, colonne %d)\n",
                output_file, error.text, error.line, error.column);
        return;
    }

    json_t *routes = json_object_get(root, "routes");
    if (!routes || !json_is_array(routes))
    {
        routes = json_array();
        json_object_set(root, "routes", routes);
    }

    // Création de l'objet route
    json_t *route_obj = json_object();
    json_object_set_new(route_obj, "network", json_string(new_route->network));
    json_object_set_new(route_obj, "mask", json_string(new_route->mask));
    json_object_set_new(route_obj, "gateway", json_string(new_route->gateway));
    json_object_set_new(route_obj, "hop", json_integer(new_route->hop));
    json_object_set_new(route_obj, "next_hop", json_string(new_route->next_hop));

    // Vérifie si une route identique existe déjà (network + mask + gateway)
    size_t index;
    json_t *value;
    json_array_foreach(routes, index, value)
    {
        const char *existing_network = json_string_value(json_object_get(value, "network"));
        const char *existing_mask = json_string_value(json_object_get(value, "mask"));
        const char *existing_gateway = json_string_value(json_object_get(value, "gateway"));

        if (existing_network && existing_mask && existing_gateway &&
            strcmp(existing_network, new_route->network) == 0 &&
            strcmp(existing_mask, new_route->mask) == 0 &&
            strcmp(existing_gateway, new_route->gateway) == 0)
        {
            // Met à jour l'entrée existante
            json_array_set(routes, index, route_obj);
            goto save_and_exit;
        }
    }

    // Si aucune entrée ne correspond, on ajoute
    json_array_append_new(routes, route_obj);

save_and_exit:
    if (json_dump_file(root, output_file, JSON_INDENT(4)) != 0)
    {
        fprintf(stderr, "Erreur lors de la mise à jour JSON\n");
    }

    json_decref(root);
}

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

    // Initialisation du json
    Route route;
    strncpy(route.network, "10.0.12.0", sizeof(route.network));
    strncpy(route.mask, "255.255.255.0", sizeof(route.mask));
    strncpy(route.gateway, "eth0", sizeof(route.gateway));
    route.hop = 1;
    init_json("router_info.json", &route);
    strncpy(route.network, "10.0.1.0", sizeof(route.network));
    strncpy(route.mask, "255.255.255.0", sizeof(route.mask));
    strncpy(route.gateway, "eth1", sizeof(route.gateway));
    route.hop = 1;
    init_json("router_info.json", &route);

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
