#include <unistd.h>     // pour gethostname(), HOST_NAME_MAX
#include <limits.h>     // pour HOST_NAME_MAX (parfois défini ici)
#include <sys/types.h>  // pour ifaddrs
#include <ifaddrs.h>    // pour getifaddrs(), struct ifaddrs
#include <arpa/inet.h>  // pour inet_ntoa(), inet_aton(), struct in_addr, INET_ADDRSTRLEN
#include <netinet/in.h> // pour struct sockaddr_in
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <net/if.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>

#include "lsdb.h"
#include "return.h"

ReturnCode init_json(const char *output_file)
{
    json_t *root = json_object();
    if (!root)
    {
        fprintf(stderr, "Erreur lors de la création de l'objet JSON\n");
        return JSON_DUMP_ERROR;
    }

    // Obtenir le nom d'hôte de la machine
    char hostname[HOST_NAME_MAX + 1];
    if (gethostname(hostname, sizeof(hostname)) != 0)
    {
        perror("gethostname");
        json_decref(root);
        return HOSTNAME_ERROR;
    }

    // Remplir les champs de base
    json_object_set_new(root, "name", json_string(hostname));
    json_object_set_new(root, "connected", json_array());
    json_object_set_new(root, "neighbors", json_array());
    json_object_set_new(root, "sequence", json_integer(1));

    // Écrire dans le fichier
    if (json_dump_file(root, output_file, JSON_INDENT(4)) != 0)
    {
        fprintf(stderr, "Erreur lors de l'écriture dans le fichier JSON '%s'\n", output_file);
        json_decref(root);
        return JSON_WRITE_ERROR;
    }

    // Nettoyage
    json_decref(root);
    return JSON_WRITE_SUCCESS;
}

ReturnCode print_json_connected(const char *output_file)
{
    json_error_t error;
    json_t *root = json_load_file(output_file, 0, &error);
    if (!root)
    {
        fprintf(stderr, "Erreur lors de l'ouverture du fichier JSON '%s' : %s (ligne %d, colonne %d)\n",
                output_file, error.text, error.line, error.column);
        return JSON_LOAD_ERROR;
    }
    json_t *connected = json_object_get(root, "connected");
    if (!connected || !json_is_array(connected))
    {
        connected = json_array();
        json_object_set(root, "connected", connected);
    }

    printf("\n===== Interfaces réseaux détectées =====\n");
    struct ifaddrs *ifap = NULL, *ifa = NULL;
    if (getifaddrs(&ifap) != 0)
    {
        perror("getifaddrs failed");
        return INTERFACE_NOT_FOUND;
    }

    for (ifa = ifap; ifa != NULL; ifa = ifa->ifa_next)
    {
        if (!ifa->ifa_addr || ifa->ifa_addr->sa_family != AF_INET)
            continue;
        if (strcmp(ifa->ifa_name, "lo") == 0)
            continue;

        struct sockaddr_in *sa = (struct sockaddr_in *)ifa->ifa_addr;
        struct sockaddr_in *nm = (struct sockaddr_in *)ifa->ifa_netmask;
        char ip[INET_ADDRSTRLEN] = {0};
        char netmask[INET_ADDRSTRLEN] = "N/A";
        char network[INET_ADDRSTRLEN] = "N/A";

        strncpy(ip, inet_ntoa(sa->sin_addr), INET_ADDRSTRLEN - 1);
        if (ifa->ifa_netmask)
        {
            strncpy(netmask, inet_ntoa(nm->sin_addr), INET_ADDRSTRLEN - 1);
            struct in_addr net_addr;
            net_addr.s_addr = sa->sin_addr.s_addr & nm->sin_addr.s_addr;
            strncpy(network, inet_ntoa(net_addr), INET_ADDRSTRLEN - 1);
        }

        // >>> Crée un objet distinct pour chaque interface
        json_t *connected_obj = json_object();
        json_object_set_new(connected_obj, "network", json_string(network));
        json_object_set_new(connected_obj, "mask", json_string(netmask));
        json_object_set_new(connected_obj, "gateway", json_string(ifa->ifa_name));
        json_object_set_new(connected_obj, "hop", json_integer(1));
        json_array_append_new(connected, connected_obj);

        printf("Interface: %s\tIP: %s\tNetmask: %s\tNetwork: %s\n", ifa->ifa_name, ip, netmask, network);
    }

    freeifaddrs(ifap);
    json_object_set(root, "connected", connected);

    // Incrémentation du champ sequence
    json_t *seq = json_object_get(root, "sequence");
    int sequence = seq ? json_integer_value(seq) : 1;
    json_object_set(root, "sequence", json_integer(sequence + 1));

    if (json_dump_file(root, JSON_FILE_NAME, JSON_INDENT(4)) != 0)
    {
        fprintf(stderr, "Erreur lors de la mise à jour JSON\n");
        return JSON_DUMP_ERROR;
    }

    json_decref(root);
    return JSON_WRITE_SUCCESS;
}

// Utilitaire : vérifie si un réseau existe déjà dans une liste json
ReturnCode route_exists(json_t *array, const char *network, const char *mask)
{
    size_t i;
    json_t *elem;
    json_array_foreach(array, i, elem)
    {
        const char *net = json_string_value(json_object_get(elem, "network"));
        const char *msk = json_string_value(json_object_get(elem, "mask"));
        if (net && msk && strcmp(net, network) == 0 && strcmp(msk, mask) == 0)
            return INTERFACE_NOT_FOUND;
    }
    return RETURN_SUCCESS;
}

// Vérifie si un voisin existe déjà avec network, mask, gateway, next_hop
int neighbor_entry_exists(json_t *array, const char *network, const char *mask, const char *gateway, const char *next_hop)
{
    size_t i;
    json_t *elem;
    json_array_foreach(array, i, elem)
    {
        const char *net = json_string_value(json_object_get(elem, "network"));
        const char *msk = json_string_value(json_object_get(elem, "mask"));
        const char *gw = json_string_value(json_object_get(elem, "gateway"));
        const char *nh = json_string_value(json_object_get(elem, "next_hop"));
        if (net && msk && gw && nh &&
            strcmp(net, network) == 0 &&
            strcmp(msk, mask) == 0 &&
            strcmp(gw, gateway) == 0 &&
            strcmp(nh, next_hop) == 0)
            return 1;
    }
    return 0;
}

// Trouve l'interface de R1 (gateway) utilisée pour atteindre une IP (next_hop)
const char *find_gateway_interface(json_t *connected, const char *ip)
{
    size_t i;
    json_t *entry;
    struct in_addr addr_ip, addr_net, addr_mask, addr_calc;

    if (!inet_aton(ip, &addr_ip))
        return NULL;

    json_array_foreach(connected, i, entry)
    {
        const char *network = json_string_value(json_object_get(entry, "network"));
        const char *mask = json_string_value(json_object_get(entry, "mask"));
        const char *gateway = json_string_value(json_object_get(entry, "gateway"));
        if (!network || !mask || !gateway)
            continue;

        inet_aton(network, &addr_net);
        inet_aton(mask, &addr_mask);

        addr_calc.s_addr = addr_ip.s_addr & addr_mask.s_addr;
        if (addr_calc.s_addr == addr_net.s_addr)
            return gateway;
    }

    return NULL;
}

ReturnCode print_json_neighbors(const char *my_json_file, const char *peer_ip, const char *peer_json_text)
{
    json_error_t error;
    json_t *my_root = json_load_file(my_json_file, 0, &error);
    json_t *peer_root = json_loads(peer_json_text, 0, &error);

    if (!my_root || !peer_root)
    {
        fprintf(stderr, "Erreur lors du chargement JSON : %s\n", error.text);
        return RETURN_UNKNOWN_ERROR;
    }

    json_t *my_connected = json_object_get(my_root, "connected");
    json_t *my_neighbors = json_object_get(my_root, "neighbors");
    if (!json_is_array(my_connected) || !json_is_array(my_neighbors))
    {
        fprintf(stderr, "Champs 'connected' ou 'neighbors' manquants ou invalides.\n");
        return RETURN_UNKNOWN_ERROR;
    }

    json_t *peer_connected = json_object_get(peer_root, "connected");
    json_t *peer_neighbors = json_object_get(peer_root, "neighbors");
    if (!json_is_array(peer_connected))
        return RETURN_UNKNOWN_ERROR;

    int modification = 0;

    // 1. Ajout des réseaux "connected" du peer (hop=2)
    size_t i;
    json_t *peer_route;
    json_array_foreach(peer_connected, i, peer_route)
    {
        const char *network = json_string_value(json_object_get(peer_route, "network"));
        const char *mask = json_string_value(json_object_get(peer_route, "mask"));
        if (!network || !mask)
            continue;

        const char *gateway = find_gateway_interface(my_connected, peer_ip);
        if (!gateway)
            continue;
        // next_hop = peer_ip
        if (neighbor_entry_exists(my_neighbors, network, mask, gateway, peer_ip))
            continue;

        json_t *neighbor = json_object();
        json_object_set_new(neighbor, "network", json_string(network));
        json_object_set_new(neighbor, "mask", json_string(mask));
        json_object_set_new(neighbor, "gateway", json_string(gateway));
        json_object_set_new(neighbor, "hop", json_integer(2));
        json_object_set_new(neighbor, "next_hop", json_string(peer_ip));
        json_array_append_new(my_neighbors, neighbor);
        modification = 1;
    }

    // 2. Ajout des réseaux "neighbors" du peer (hop=3)
    if (json_is_array(peer_neighbors))
    {
        size_t j;
        json_t *peer_neighbor;
        json_array_foreach(peer_neighbors, j, peer_neighbor)
        {
            const char *network = json_string_value(json_object_get(peer_neighbor, "network"));
            const char *mask = json_string_value(json_object_get(peer_neighbor, "mask"));
            const char *peer_next_hop = json_string_value(json_object_get(peer_neighbor, "next_hop"));
            int peer_hop = json_integer_value(json_object_get(peer_neighbor, "hop"));
            if (!network || !mask || !peer_next_hop)
                continue;

            const char *gateway = find_gateway_interface(my_connected, peer_ip);
            if (!gateway)
                continue;
            // next_hop = peer_ip
            if (neighbor_entry_exists(my_neighbors, network, mask, gateway, peer_ip))
                continue;

            json_t *neighbor = json_object();
            json_object_set_new(neighbor, "network", json_string(network));
            json_object_set_new(neighbor, "mask", json_string(mask));
            json_object_set_new(neighbor, "gateway", json_string(gateway));
            json_object_set_new(neighbor, "hop", json_integer(peer_hop + 1));
            json_object_set_new(neighbor, "next_hop", json_string(peer_ip));
            json_array_append_new(my_neighbors, neighbor);
            modification = 1;
        }
    }

    // Incrémentation du champ sequence uniquement si modification
    if (modification)
    {
        json_t *seq = json_object_get(my_root, "sequence");
        int sequence = seq ? json_integer_value(seq) : 1;
        json_object_set(my_root, "sequence", json_integer(sequence + 1));
    }

    json_dump_file(my_root, my_json_file, JSON_INDENT(4));
    json_decref(my_root);
    json_decref(peer_root);

    return RETURN_SUCCESS;
}

// Met à jour is_Ospf à 1 pour l'interface donnée dans le JSON
ReturnCode set_is_ospf(const char *json_file, const char *interface_name)
{
    json_error_t error;
    json_t *root = json_load_file(json_file, 0, &error);
    if (!root)
    {
        fprintf(stderr, "Erreur lors de l'ouverture du fichier JSON '%s' : %s\n", json_file, error.text);
        return JSON_LOAD_ERROR;
    }
    json_t *connected = json_object_get(root, "connected");
    if (!connected || !json_is_array(connected))
    {
        json_decref(root);
        return JSON_LOAD_ERROR;
    }
    size_t i;
    json_t *entry;
    int found = 0;
    json_array_foreach(connected, i, entry)
    {
        const char *gateway = json_string_value(json_object_get(entry, "gateway"));
        if (gateway && strcmp(gateway, interface_name) == 0)
        {
            json_object_set_new(entry, "is_Ospf", json_integer(1));
            found = 1;
            break;
        }
    }
    if (!found)
    {
        json_decref(root);
        return INTERFACE_NOT_FOUND;
    }
    // Incrémentation du champ sequence
    json_t *seq = json_object_get(root, "sequence");
    int sequence = seq ? json_integer_value(seq) : 1;
    json_object_set(root, "sequence", json_integer(sequence + 1));

    if (json_dump_file(root, json_file, JSON_INDENT(4)) != 0)
    {
        json_decref(root);
        return JSON_DUMP_ERROR;
    }
    json_decref(root);
    return RETURN_SUCCESS;
}

ReturnCode send_json_to_ospf_neighbors(const char *json_file)
{
    static int last_sequence_sent = -1; // static pour garder la valeur entre les appels

    json_error_t error;
    json_t *root = json_load_file(json_file, 0, &error);
    if (!root)
    {
        fprintf(stderr, "Erreur lors de l'ouverture du fichier JSON '%s' : %s\n", json_file, error.text);
        return JSON_LOAD_ERROR;
    }
    json_t *seq = json_object_get(root, "sequence");
    int sequence = seq ? json_integer_value(seq) : 1;

    if (sequence == last_sequence_sent)
    {
        json_decref(root);
        printf("Aucun changement de sequence, propagation OSPF non nécessaire.\n");
        return RETURN_SUCCESS;
    }
    last_sequence_sent = sequence;

    json_t *connected = json_object_get(root, "connected");
    if (!connected || !json_is_array(connected))
    {
        json_decref(root);
        return JSON_LOAD_ERROR;
    }

    // Lire le JSON à envoyer
    char *json_str = NULL;
    FILE *f = fopen(json_file, "r");
    if (f)
    {
        fseek(f, 0, SEEK_END);
        long json_len = ftell(f);
        fseek(f, 0, SEEK_SET);
        json_str = malloc(json_len + 1);
        fread(json_str, 1, json_len, f);
        json_str[json_len] = '\0';
        fclose(f);
    }
    else
    {
        json_decref(root);
        return JSON_LOAD_ERROR;
    }

    // Pour chaque voisin OSPF, envoyer le JSON
    size_t i;
    json_t *entry;
    int sent_count = 0;
    json_array_foreach(connected, i, entry)
    {
        json_t *is_ospf_field = json_object_get(entry, "is_Ospf");
        int is_ospf = is_ospf_field ? json_integer_value(is_ospf_field) : 0;
        const char *network = json_string_value(json_object_get(entry, "network"));
        const char *mask = json_string_value(json_object_get(entry, "mask"));
        if (is_ospf == 1 && network && mask)
        {
            struct in_addr net_addr, mask_addr, peer_addr;
            inet_aton(network, &net_addr);
            inet_aton(mask, &mask_addr);
            uint32_t net = ntohl(net_addr.s_addr);
            uint32_t msk = ntohl(mask_addr.s_addr);
            uint32_t peer_ip = (net & msk) + 1;
            peer_addr.s_addr = htonl(peer_ip);
            char peer_ip_str[INET_ADDRSTRLEN];
            inet_ntop(AF_INET, &peer_addr, peer_ip_str, INET_ADDRSTRLEN);

            int sock = socket(AF_INET, SOCK_STREAM, 0);
            if (sock < 0)
                continue;
            struct sockaddr_in addr;
            memset(&addr, 0, sizeof(addr));
            addr.sin_family = AF_INET;
            addr.sin_port = htons(PORT);
            addr.sin_addr = peer_addr;

            struct timeval timeout;
            timeout.tv_sec = 1;
            timeout.tv_usec = 0;
            setsockopt(sock, SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof(timeout));
            if (connect(sock, (struct sockaddr *)&addr, sizeof(addr)) == 0)
            {
                send(sock, json_str, strlen(json_str), 0);
                sent_count++;
            }
            close(sock);
        }
    }
    free(json_str);
    json_decref(root);
    printf("JSON envoyé à %d voisin(s) OSPF.\n", sent_count);
    return RETURN_SUCCESS;
}

int route_exists_and_is_better(Route *routes, int count, Route *new_route)
{
    for (int i = 0; i < count; ++i)
    {
        if (strcmp(routes[i].network, new_route->network) == 0 &&
            strcmp(routes[i].mask, new_route->mask) == 0)
        {
            // Si on trouve une meilleure route (moins de hops), on remplace
            if (new_route->hop < routes[i].hop)
            {
                routes[i] = *new_route;
            }
            return 1;
        }
    }
    return 0;
}

int generate_routing_table_from_file(const char *filename, Route *routing_table)
{
    json_error_t error;
    json_t *root = json_load_file(filename, 0, &error);
    if (!root)
    {
        fprintf(stderr, "Erreur de lecture JSON : %s\n", error.text);
        return -1;
    }

    int count = 0;

    // 1. Connected routes
    json_t *connected = json_object_get(root, "connected");
    if (!json_is_array(connected))
    {
        fprintf(stderr, "Erreur : 'connected' n'est pas un tableau JSON.\n");
        json_decref(root);
        return -1;
    }

    size_t i;
    json_t *entry;
    json_array_foreach(connected, i, entry)
    {
        Route r;
        snprintf(r.network, sizeof(r.network), "%s", json_string_value(json_object_get(entry, "network")));
        snprintf(r.mask, sizeof(r.mask), "%s", json_string_value(json_object_get(entry, "mask")));
        snprintf(r.gateway, sizeof(r.gateway), "%s", json_string_value(json_object_get(entry, "gateway")));
        r.hop = json_integer_value(json_object_get(entry, "hop"));
        r.is_connected = 1;
        strcpy(r.next_hop, "");
        routing_table[count++] = r;
    }

    // 2. Neighbors (best only)
    json_t *neighbors = json_object_get(root, "neighbors");
    if (!json_is_array(neighbors))
    {
        fprintf(stderr, "Erreur : 'neighbors' n'est pas un tableau JSON.\n");
        json_decref(root);
        return -1;
    }

    json_array_foreach(neighbors, i, entry)
    {
        Route r;
        snprintf(r.network, sizeof(r.network), "%s", json_string_value(json_object_get(entry, "network")));
        snprintf(r.mask, sizeof(r.mask), "%s", json_string_value(json_object_get(entry, "mask")));
        snprintf(r.gateway, sizeof(r.gateway), "%s", json_string_value(json_object_get(entry, "gateway")));
        snprintf(r.next_hop, sizeof(r.next_hop), "%s", json_string_value(json_object_get(entry, "next_hop")));
        r.hop = json_integer_value(json_object_get(entry, "hop"));
        r.is_connected = 0;

        if (!route_exists_and_is_better(routing_table, count, &r))
        {
            routing_table[count++] = r;
        }
    }

    json_decref(root);
    return count;
}
