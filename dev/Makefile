# Variables
CC = gcc
CFLAGS = -Wall -Wextra -I.
BUILD_DIR = build
LDFLAGS = -ljansson

CLIENT_SRC = Client/client.c
SERVER_SRC = Server/server.c
SERVER_R1_SRC = Server/server_r1.c
MAIN_SRC = main.c

CLIENT_BIN = $(BUILD_DIR)/client
SERVER_BIN = $(BUILD_DIR)/server
SERVER_R1_BIN = $(BUILD_DIR)/server_r1
MAIN_BIN = $(BUILD_DIR)/main

# Cibles par défaut
all: $(CLIENT_BIN) $(SERVER_BIN) $(SERVER_R1_BIN)

# Création du dossier de build s'il n'existe pas
$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

# Compilation du client
$(CLIENT_BIN): $(CLIENT_SRC) hello.h | $(BUILD_DIR)
	$(CC) $(CFLAGS) -o $@ $(CLIENT_SRC) $(LDFLAGS)

# Compilation du serveur
$(SERVER_BIN): $(SERVER_SRC) hello.h | $(BUILD_DIR)
	$(CC) $(CFLAGS) -o $@ $(SERVER_SRC) $(LDFLAGS)

# Compilation du serveur_r1 (CORRIGÉ)
$(SERVER_R1_BIN): $(SERVER_R1_SRC) hello.h | $(BUILD_DIR)
	$(CC) $(CFLAGS) -o $@ $(SERVER_R1_SRC) $(LDFLAGS)

# Nettoyage
clean:
	rm -rf $(BUILD_DIR)

.PHONY: all clean