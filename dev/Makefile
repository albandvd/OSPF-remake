# CC=gcc
# CFLAGS=-Wall -g
# TARGET=test

# all: $(TARGET)

# $(TARGET): main.c
# 	$(CC) $(CFLAGS) -o $(TARGET) main.c

# clean:
# 	rm -f $(TARGET)
# Variables
CC = gcc
CFLAGS = -Wall -Wextra -O2

# Dossiers
CLIENT_DIR = Client
SERVER_DIR = Server

# Fichiers sources
CLIENT_SRC = $(CLIENT_DIR)/client.c
SERVER_SRC = $(SERVER_DIR)/server.c

# Fichiers objets
CLIENT_OBJ = $(CLIENT_SRC:.c=.o)
SERVER_OBJ = $(SERVER_SRC:.c=.o)

# Exécutables
CLIENT_BIN = client
SERVER_BIN = server

# Cibles par défaut
all: $(CLIENT_BIN) $(SERVER_BIN)

# Compilation du client
$(CLIENT_BIN): $(CLIENT_OBJ)
	$(CC) $(CFLAGS) -o $@ $^

# Compilation du serveur
$(SERVER_BIN): $(SERVER_OBJ)
	$(CC) $(CFLAGS) -o $@ $^

# Nettoyage
clean:
	$(RM) $(CLIENT_DIR)/*.o $(SERVER_DIR)/*.o $(CLIENT_BIN) $(SERVER_BIN)

.PHONY: all clean
