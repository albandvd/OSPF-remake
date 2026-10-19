#include <stdio.h>
#include <string.h>

#include "lsdb.h"
#include "return.h"
#include "check-service.h"
#include "log.h"

/* Usage: interface [add|delete] <interface_name>
 *
 * add    — enroll the interface in the OSPF protocol
 * delete — remove the interface from the OSPF protocol
 *
 * The daemon (main) picks up the change on its next discovery cycle.
 */

/* Look up the network/mask of a connected interface, for a readable log line */
static void describe_interface(const char *interface_name, char *out, size_t out_len)
{
    snprintf(out, out_len, "%s", interface_name);

    json_error_t error;
    json_t *root = json_load_file(JSON_FILE_NAME, 0, &error);
    if (!root)
        return;

    json_t *connected = json_object_get(root, "connected");
    if (connected && json_is_array(connected))
    {
        size_t i;
        json_t *entry;
        json_array_foreach(connected, i, entry)
        {
            const char *gw = lsdb_jstr(json_object_get(entry, "gateway"), NULL);
            if (gw && strcmp(gw, interface_name) == 0)
            {
                const char *network = lsdb_jstr(json_object_get(entry, "network"), "?");
                const char *mask    = lsdb_jstr(json_object_get(entry, "mask"), "?");
                snprintf(out, out_len, "%s %s/%s", interface_name, network, mask);
                break;
            }
        }
    }
    json_decref(root);
}

int main(int argc, char *argv[])
{
    log_init();

    if (argc < 3)
    {
        fprintf(stderr, "Usage: %s [add|delete] <interface_name>\n", argv[0]);
        return ERROR_INVALID_COMMAND;
    }

    const char *command   = argv[1];
    const char *interface = argv[2];

    if (checkservice() == SERVICE_NOT_LAUNCHED)
    {
        fprintf(stderr, "Error: the OSPF daemon is not running (lsdb.json missing).\n");
        return SERVICE_NOT_LAUNCHED;
    }

    if (strcmp(command, "add") == 0)
    {
        ReturnCode rc = set_is_ospf(JSON_FILE_NAME, interface);
        if (rc == INTERFACE_NOT_FOUND)
        {
            fprintf(stderr, "Error: interface '%s' not found in lsdb.json.\n", interface);
            return rc;
        }
        if (rc != RETURN_SUCCESS)
        {
            fprintf(stderr, "Error: %s\n", return_code_to_string(rc));
            return rc;
        }
        char description[80];
        describe_interface(interface, description, sizeof(description));
        log_event("INTERFACE ENABLED", "%s", description);
        printf("Interface '%s' added to the OSPF protocol.\n", interface);
    }
    else if (strcmp(command, "delete") == 0)
    {
        ReturnCode rc = unset_is_ospf(JSON_FILE_NAME, interface);
        if (rc == INTERFACE_NOT_FOUND)
        {
            fprintf(stderr, "Error: interface '%s' not found in lsdb.json.\n", interface);
            return rc;
        }
        if (rc != RETURN_SUCCESS)
        {
            fprintf(stderr, "Error: %s\n", return_code_to_string(rc));
            return rc;
        }
        log_event("INTERFACE DISABLED", "%s", interface);
        printf("Interface '%s' removed from the OSPF protocol.\n", interface);
    }
    else
    {
        fprintf(stderr, "Invalid command. Use 'add' or 'delete'.\n");
        return ERROR_INVALID_COMMAND;
    }

    return RETURN_SUCCESS;
}
