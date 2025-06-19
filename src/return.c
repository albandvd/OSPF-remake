#include "return.h"

const char* return_code_to_string(ReturnCode code) {
    switch (code) {
        case RETURN_SUCCESS: return "Success";
        case RETURN_UNKNOWN_ERROR: return "Unknown error";

        // Service
        case SERVICE_LAUNCHED: return "Service launched";
        case SERVICE_NOT_LAUNCHED: return "Service not launched";

        // LSDB
        case LSDB_ERROR_FILE_NOT_FOUND: return "LSDB: File not found";
        case LSDB_ERROR_READ_FAILURE: return "LSDB: Read failure";
        case LSDB_ERROR_INVALID_COMMAND: return "LSDB: Invalid command";
        case LSDB_ERROR_INTERFACE_NOT_FOUND: return "LSDB: Interface not found";
        case LSDB_ERROR_SERVICE_NOT_AVAILABLE: return "LSDB: Service not available";
        case LSDB_ERROR_NULL_ARGUMENT: return "LSDB: Null argument";

        // Interface
        case INTERFACE_NOT_FOUND: return "Interface not found";
        case INTERFACE_ALREADY_EXISTS: return "Interface already exists";
        case INTERFACE_ADDED: return "Interface added successfully";
        case INTERFACE_DELETED: return "Interface deleted successfully";

        // Fichier / I/O
        case FILE_OPEN_ERROR: return "File open error";
        case FILE_WRITE_ERROR: return "File write error";
        case FILE_READ_ERROR: return "File read error";
        case FILE_CLOSE_ERROR: return "File close error";

        default: return "Unlisted return code";
    }
}
