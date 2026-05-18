#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <arpa/inet.h>
#include <jansson.h>

static void show_neighbors(void)
{
    json_error_t error;
    json_t *root = json_load_file("lsdb.json", 0, &error);
    if (!root)
    {
        fprintf(stderr, "Erreur de lecture de lsdb.json : %s\n", error.text);
        return;
    }

    const char *my_name = json_string_value(json_object_get(root, "name"));
    printf("Routeur : %s\n\n", my_name ? my_name : "inconnu");

    json_t *neighbors = json_object_get(root, "neighbors");
    if (!neighbors || !json_is_array(neighbors) || json_array_size(neighbors) == 0)
    {
        printf("Aucun voisin OSPF connu.\n");
        json_decref(root);
        return;
    }

    /* Deduplicate by next_hop to show one line per neighbor router */
    printf("%-20s %-20s %-10s %s\n",
           "Nom routeur", "Next-hop IP", "Hops", "Interface");
    printf("%-20s %-20s %-10s %s\n",
           "--------------------", "--------------------", "----------", "---------");

    char seen[64][INET_ADDRSTRLEN];
    int seen_count = 0;

    size_t index;
    json_t *value;
    json_array_foreach(neighbors, index, value)
    {
        const char *name     = json_string_value(json_object_get(value, "name"));
        const char *next_hop = json_string_value(json_object_get(value, "next_hop"));
        const char *gateway  = json_string_value(json_object_get(value, "gateway"));
        int hop              = (int)json_integer_value(json_object_get(value, "hop"));

        if (!next_hop) continue;

        /* Skip duplicates (same next_hop already printed) */
        int already = 0;
        for (int i = 0; i < seen_count; i++)
        {
            if (strcmp(seen[i], next_hop) == 0) { already = 1; break; }
        }
        if (already) continue;

        if (seen_count < 64)
        {
            strncpy(seen[seen_count], next_hop, INET_ADDRSTRLEN - 1);
            seen[seen_count][INET_ADDRSTRLEN - 1] = '\0';
            seen_count++;
        }

        printf("%-20s %-20s %-10d %s\n",
               name    ? name    : "?",
               next_hop,
               hop,
               gateway ? gateway : "?");
    }

    json_decref(root);
}

static void show_routes(void)
{
    json_error_t error;
    json_t *root = json_load_file("lsdb.json", 0, &error);
    if (!root)
    {
        fprintf(stderr, "Erreur de lecture de lsdb.json : %s\n", error.text);
        return;
    }

    printf("%-20s %-20s %-20s %-10s %s\n",
           "Réseau", "Masque", "Next-hop", "Hops", "Interface");
    printf("%-20s %-20s %-20s %-10s %s\n",
           "--------------------", "--------------------",
           "--------------------", "----------", "---------");

    json_t *connected = json_object_get(root, "connected");
    if (json_is_array(connected))
    {
        size_t i;
        json_t *e;
        json_array_foreach(connected, i, e)
        {
            printf("%-20s %-20s %-20s %-10s %s\n",
                   json_string_value(json_object_get(e, "network")),
                   json_string_value(json_object_get(e, "mask")),
                   "directly connected",
                   "0",
                   json_string_value(json_object_get(e, "gateway")));
        }
    }

    json_t *neighbors = json_object_get(root, "neighbors");
    if (json_is_array(neighbors))
    {
        size_t i;
        json_t *e;
        json_array_foreach(neighbors, i, e)
        {
            printf("%-20s %-20s %-20s %-10d %s\n",
                   json_string_value(json_object_get(e, "network")),
                   json_string_value(json_object_get(e, "mask")),
                   json_string_value(json_object_get(e, "next_hop")),
                   (int)json_integer_value(json_object_get(e, "hop")),
                   json_string_value(json_object_get(e, "gateway")));
        }
    }

    json_decref(root);
}

int main(int argc, char *argv[])
{
    if (argc == 2 && strcmp(argv[1], "neighbors") == 0)
    {
        show_neighbors();
        return 0;
    }
    if (argc == 2 && strcmp(argv[1], "routes") == 0)
    {
        show_routes();
        return 0;
    }
    printf("Usage: %s [neighbors|routes]\n", argv[0]);
    return 1;
}
