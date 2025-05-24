#include <stdio.h>

int main() {
    printf("Hello, world\n");

    // Création du fichier install.txt
    FILE *file = fopen("install.txt", "w");
    if (file != NULL) {
        fclose(file);
        printf("install.txt créé.\n");
    } else {
        printf("Erreur lors de la création de install.txt.\n");
    }

    return 0;
}