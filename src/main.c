#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <net/if.h>

#include "lsdb.h"
#include "return.h"
#include "route.h"
#include "exchanges.h"

static void apply_routes(void)
{
    Route table[MAX_ROUTES];
    int count = generate_routing_table_from_file(JSON_FILE_NAME, table);
    if (count < 0)
        return;

    for (int i = 0; i < count; i++)
    {
        if (table[i].is_connected)
            continue;
        if (table[i].next_hop[0] == '\0')
            continue;

        ReturnCode rc = add_route(table[i].network, table[i].mask,
                                  table[i].next_hop, table[i].gateway);
        if (rc == ROUTE_ADDED_SUCCESSFULLY)
            printf("[ospf] route %s/%s via %s (%s) hop=%d ajoutée\n",
                   table[i].network, table[i].mask,
                   table[i].next_hop, table[i].gateway,
                   table[i].hop);
    }
}

/* Delete routes present in old_table but absent from new_table */
static void delete_stale_routes(Route *old_table, int old_count,
                                 Route *new_table, int new_count)
{
    for (int i = 0; i < old_count; i++)
    {
        if (old_table[i].is_connected || old_table[i].next_hop[0] == '\0')
            continue;

        int found = 0;
        for (int j = 0; j < new_count; j++)
        {
            if (!new_table[j].is_connected &&
                strcmp(old_table[i].network,  new_table[j].network)  == 0 &&
                strcmp(old_table[i].mask,     new_table[j].mask)     == 0 &&
                strcmp(old_table[i].next_hop, new_table[j].next_hop) == 0)
            {
                found = 1;
                break;
            }
        }
        if (!found)
            delete_route(old_table[i].network, old_table[i].mask,
                         old_table[i].next_hop, old_table[i].gateway);
    }
}

int main(void)
{
    /* Initialise la LSDB et détecte les interfaces locales */
    init_json(JSON_FILE_NAME);
    print_json_connected(JSON_FILE_NAME);

    printf("[ospf] daemon démarré sur le port %d\n", PORT);

    /* Fork le processus serveur (écoute sur port 4242) */
    pid_t pid = fork();
    if (pid < 0)
    {
        perror("fork");
        return EXIT_FAILURE;
    }
    if (pid == 0)
    {
        run_server();   /* ne retourne jamais */
        exit(0);
    }

    /* Boucle principale : re-découverte périodique des voisins */
    while (1)
    {
        sleep(OSPF_INTERVAL);

        /* Snapshot des routes actuellement installées (pour nettoyage après scan) */
        Route old_routes[MAX_ROUTES];
        int old_count = generate_routing_table_from_file(JSON_FILE_NAME, old_routes);

        /* Efface les voisins connus et re-découvre (routes noyau conservées pendant le scan) */
        clear_neighbors(JSON_FILE_NAME);
        char interfaces[MAX_IFACE][IFNAMSIZ];
        int iface_count = get_ospf_interfaces(JSON_FILE_NAME, interfaces, MAX_IFACE);
        for (int i = 0; i < iface_count; i++)
            scan_subnet_for_ospf(interfaces[i]);

        /* Installe les nouvelles routes (ip route replace est idempotent) */
        apply_routes();

        /* Supprime les routes devenues obsolètes */
        if (old_count > 0)
        {
            Route new_routes[MAX_ROUTES];
            int new_count = generate_routing_table_from_file(JSON_FILE_NAME, new_routes);
            delete_stale_routes(old_routes, old_count, new_routes, new_count);
        }
    }

    return RETURN_SUCCESS;
}
