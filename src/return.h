#ifndef RETURN_H
#define RETURN_H

typedef enum {
    // ---- Général / succès ----
    RETURN_SUCCESS = 0,
    RETURN_UNKNOWN_ERROR,
    ERROR_INVALID_COMMAND,
    ERROR_INVALID_ARGUMENT,

    // ---- Service ----
    SERVICE_LAUNCHED,
    SERVICE_NOT_LAUNCHED,

    // ---- LSDB spécifiques ----
    LSDB_ERROR_FILE_NOT_FOUND,
    LSDB_ERROR_READ_FAILURE,
    LSDB_ERROR_INVALID_COMMAND,
    LSDB_ERROR_INTERFACE_NOT_FOUND,
    LSDB_ERROR_SERVICE_NOT_AVAILABLE,
    LSDB_ERROR_NULL_ARGUMENT,
    LSDB_ERROR_FULL_LSA,
    LSDB_ERROR_LSA_NOT_FOUND,

    // ---- Réseau / Interface ----
    INTERFACE_NOT_FOUND,
    INTERFACE_ALREADY_EXISTS,
    INTERFACE_ADDED,
    INTERFACE_DELETED,
    SYSTEM_CALL_FAILURE,

    // ---- Fichier / I/O ----
    FILE_OPEN_ERROR,
    FILE_WRITE_ERROR,
    FILE_READ_ERROR,
    FILE_CLOSE_ERROR,

    // ---- Autres modules... ajouter ici ----

} ReturnCode;

// Fonction utilitaire pour obtenir un message lisible
const char* return_code_to_string(ReturnCode code);

#endif // RETURN_H
