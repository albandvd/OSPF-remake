#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <jansson.h> // Nécessite la bibliothèque jansson

void show_neighbors()
{
    json_error_t error;
    json_t *root = json_load_file("lsdb.json", 0, &error);
    if (!root)
    {
        fprintf(stderr, "Erreur de lecture de lsdb.json : %s\n", error.text);
        return;
    }

    json_t *connected = json_object_get(root, "connected");
    if (!connected || !json_is_array(connected))
    {
        fprintf(stderr, "Champ 'connected' absent ou invalide dans lsdb.json\n");
        json_decref(root);
        return;
    }

    printf("Voisins directs (connected) :\n");
    size_t index;
    json_t *value;
    json_array_foreach(connected, index, value)
    {
        if (json_is_object(value))
        {
            json_t *network = json_object_get(value, "network");
            json_t *mask = json_object_get(value, "mask");
            json_t *gateway = json_object_get(value, "gateway");
            if (network && mask && gateway &&
                json_is_string(network) && json_is_string(mask) && json_is_string(gateway))
            {
                printf("- Réseau : %s, Masque : %s, Interface : %s\n",
                       json_string_value(network),
                       json_string_value(mask),
                       json_string_value(gateway));
            }
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
    printf("Usage: %s neighbors\n", argv[0]);
    return 1;
}