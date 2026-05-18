#include <stdio.h>
#include <string.h>

#include "lsdb.h"
#include "return.h"
#include "check-service.h"

/* Usage: interface [add|delete] <interface_name>
 *
 * add    — marque l'interface comme participant au protocole OSPF
 * delete — retire l'interface du protocole OSPF
 *
 * Le daemon (main) prend en compte les changements au prochain cycle.
 */
int main(int argc, char *argv[])
{
    if (argc < 3)
    {
        fprintf(stderr, "Usage: %s [add|delete] <interface_name>\n", argv[0]);
        return ERROR_INVALID_COMMAND;
    }

    const char *command   = argv[1];
    const char *interface = argv[2];

    if (checkservice() == SERVICE_NOT_LAUNCHED)
    {
        fprintf(stderr, "Erreur: le daemon OSPF n'est pas démarré (lsdb.json absent).\n");
        return SERVICE_NOT_LAUNCHED;
    }

    if (strcmp(command, "add") == 0)
    {
        ReturnCode rc = set_is_ospf(JSON_FILE_NAME, interface);
        if (rc == INTERFACE_NOT_FOUND)
        {
            fprintf(stderr, "Erreur: interface '%s' non trouvée dans lsdb.json.\n", interface);
            return rc;
        }
        if (rc != RETURN_SUCCESS)
        {
            fprintf(stderr, "Erreur: %s\n", return_code_to_string(rc));
            return rc;
        }
        printf("Interface '%s' ajoutée au protocole OSPF.\n", interface);
    }
    else if (strcmp(command, "delete") == 0)
    {
        ReturnCode rc = unset_is_ospf(JSON_FILE_NAME, interface);
        if (rc == INTERFACE_NOT_FOUND)
        {
            fprintf(stderr, "Erreur: interface '%s' non trouvée dans lsdb.json.\n", interface);
            return rc;
        }
        if (rc != RETURN_SUCCESS)
        {
            fprintf(stderr, "Erreur: %s\n", return_code_to_string(rc));
            return rc;
        }
        printf("Interface '%s' retirée du protocole OSPF.\n", interface);
    }
    else
    {
        fprintf(stderr, "Commande invalide. Utilisez 'add' ou 'delete'.\n");
        return ERROR_INVALID_COMMAND;
    }

    return RETURN_SUCCESS;
}
