#include <unistd.h>            // pour gethostname(), HOST_NAME_MAX
#include <limits.h>            // pour HOST_NAME_MAX (parfois défini ici)
#include <sys/types.h>         // pour ifaddrs
#include <ifaddrs.h>           // pour getifaddrs(), struct ifaddrs
#include <arpa/inet.h>         // pour inet_ntoa(), inet_aton(), struct in_addr, INET_ADDRSTRLEN
#include <netinet/in.h>        // pour struct sockaddr_in
#include <string.h>  
#include <stdio.h>
#include <stdlib.h>
#include <net/if.h>

#include "lsdb.h"
#include "return.h"

ReturnCode retrieve_lsdb(LSDB *lsdb) {
    if (!lsdb) {
        fprintf(stderr, "[retrieve_lsdb] Argument LSDB NULL\n");
        return LSDB_ERROR_NULL_ARGUMENT;
    }

    FILE *f = fopen("lsdb.bin", "rb");
    if (!f) {
        // Fichier inexistant : on initialise la LSDB à vide
        memset(lsdb, 0, sizeof(LSDB));
        lsdb->countLSA = 0;
        fprintf(stderr, "[retrieve_lsdb] lsdb.bin not found, initializing empty LSDB.\n");
        return RETURN_SUCCESS;
    }

    size_t read_size = fread(lsdb, sizeof(LSDB), 1, f);
    fclose(f);

    if (read_size != 1) {
        fprintf(stderr, "[retrieve_lsdb] Erreur de lecture du fichier lsdb.bin\n");
        memset(lsdb, 0, sizeof(LSDB));
        lsdb->countLSA = 0;
        return LSDB_ERROR_READ_FAILURE;
    }

    return RETURN_SUCCESS;
}

ReturnCode init_json(const char *output_file){
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

ReturnCode print_json_connected(const char *output_file){
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
    if (json_dump_file(root, JSON_FILE_NAME, JSON_INDENT(4)) != 0)
    {
        fprintf(stderr, "Erreur lors de la mise à jour JSON\n");
        return JSON_DUMP_ERROR;
    }

    json_decref(root);
    return JSON_WRITE_SUCCESS;
}

// Utilitaire : vérifie si un réseau existe déjà dans une liste json
ReturnCode route_exists(json_t *array, const char *network, const char *mask){
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

// Trouve l'interface de R1 (gateway) utilisée pour atteindre une IP (next_hop)
const ReturnCode *find_gateway_interface(json_t *connected, const char *ip){
    size_t i;
    json_t *entry;
    struct in_addr addr_ip, addr_net, addr_mask, addr_calc;

    if (!inet_aton(ip, &addr_ip))
        return INTERFACE_NOT_FOUND;

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

    return RETURN_SUCCESS;
}

ReturnCode print_json_neighbors(const char *my_json_file, const char *peer_ip, const char *peer_json_text){
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
    if (!json_is_array(peer_connected))
        return RETURN_UNKNOWN_ERROR;

    size_t i;
    json_t *peer_route;
    json_array_foreach(peer_connected, i, peer_route)
    {
        const char *network = json_string_value(json_object_get(peer_route, "network"));
        const char *mask = json_string_value(json_object_get(peer_route, "mask"));
        if (!network || !mask)
            continue;

        if (route_exists(my_connected, network, mask))
            continue;
        if (route_exists(my_neighbors, network, mask))
            continue;

        // Ajouter aux neighbors
        const char *gateway = find_gateway_interface(my_connected, peer_ip);
        if (!gateway)
            continue;

        json_t *neighbor = json_object();
        json_object_set_new(neighbor, "network", json_string(network));
        json_object_set_new(neighbor, "mask", json_string(mask));
        json_object_set_new(neighbor, "gateway", json_string(gateway));
        json_object_set_new(neighbor, "hop", json_integer(2));
        json_object_set_new(neighbor, "next_hop", json_string(peer_ip));
        json_array_append_new(my_neighbors, neighbor);
    }

    json_dump_file(my_root, my_json_file, JSON_INDENT(4));
    json_decref(my_root);
    json_decref(peer_root);

    return RETURN_SUCCESS;
}

ReturnCode save_lsdb(LSDB *lsdb) {
    if (!lsdb) {
        ReturnCode code = LSDB_ERROR_NULL_ARGUMENT;
        fprintf(stderr, "[save_lsdb] %s\n", return_code_to_string(code));
        return code;
    }

    FILE *f = fopen("lsdb.bin", "wb");
    if (!f) {
        ReturnCode code = FILE_OPEN_ERROR;
        perror("[save_lsdb] Error opening file");
        fprintf(stderr, "%s\n", return_code_to_string(code));
        return code;
    }

    size_t written = fwrite(lsdb, sizeof(LSDB), 1, f);
    fclose(f);

    if (written != 1) {
        ReturnCode code = FILE_WRITE_ERROR;
        fprintf(stderr, "[save_lsdb] %s\n", return_code_to_string(code));
        return code;
    }

    return RETURN_SUCCESS;
}

ReturnCode add_lsa(LSA *lsa, LSDB *lsdb) {
    if (!lsa || !lsdb) {
        ReturnCode code = LSDB_ERROR_NULL_ARGUMENT;
        fprintf(stderr, "[add_lsa] %s\n", return_code_to_string(code));
        return code;
    }

    if (lsdb->countLSA >= MAX_LSAS) {
        ReturnCode code = LSDB_ERROR_FULL_LSA;
        fprintf(stderr, "[add_lsa] %s: LSDB is full\n", return_code_to_string(code));
        return code;
    }

    lsdb->lsa[lsdb->countLSA] = *lsa;
    lsdb->countLSA++;

    ReturnCode save_status = save_lsdb(lsdb);
    if (save_status != RETURN_SUCCESS) {
        fprintf(stderr, "[add_lsa] Failed to save LSDB: %s\n", return_code_to_string(save_status));
        return save_status;
    }

    return RETURN_SUCCESS;
}

ReturnCode remove_lsa(const char *routerName, const char *nameInterface, LSDB *lsdb) {
    if (!routerName || !nameInterface || !lsdb) {
        ReturnCode code = LSDB_ERROR_NULL_ARGUMENT;
        fprintf(stderr, "[remove_lsa] %s\n", return_code_to_string(code));
        return code;
    }

    int found = 0;

    for (int i = 0; i < lsdb->countLSA; ++i) {
        if (strcmp(lsdb->lsa[i].routerName, routerName) == 0 &&
            strcmp(lsdb->lsa[i].interfaces.nameInterface, nameInterface) == 0) {

            found = 1;

            // Décaler les LSA suivantes
            for (int j = i; j < lsdb->countLSA - 1; ++j) {
                lsdb->lsa[j] = lsdb->lsa[j + 1];
            }

            lsdb->countLSA--;
            break;
        }
    }

    if (!found) {
        ReturnCode code = LSDB_ERROR_LSA_NOT_FOUND;
        fprintf(stderr, "[remove_lsa] %s: (%s, %s)\n", return_code_to_string(code), routerName, nameInterface);
        return code;
    }

    ReturnCode save_status = save_lsdb(lsdb);
    if (save_status != RETURN_SUCCESS) {
        fprintf(stderr, "[remove_lsa] Failed to save LSDB: %s\n", return_code_to_string(save_status));
        return save_status;
    }

    return RETURN_SUCCESS;
}

ReturnCode get_routerId(int *routerID) {
    if (!routerID) {
        fprintf(stderr, "[get_routerId] Argument routerID NULL\n");
        return ROUTER_ID_NOT_FOUND;
    }

    FILE *f = fopen("routerID.txt", "r");
    if (!f) {
        fprintf(stderr, "[get_routerId] Error opening router_id.txt\n");
        return FILE_OPEN_ERROR;
    }

    if (fscanf(f, "%d", routerID) != 1) {
        fclose(f);
        fprintf(stderr, "[get_routerId] Error reading router ID from file\n");
        return ROUTER_ID_ERROR_READ_FAILURE;
    }

    fclose(f);
    return RETURN_SUCCESS;
}