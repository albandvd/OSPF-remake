CC = gcc
CFLAGS = -Wall -Wextra -g
LDFLAGS = -ljansson
OBJ = check-service.o lsdb.o return.o route.o exchanges.o

# Liste des exécutables à générer
BIN = main interface

all: $(BIN)

main: main.o $(OBJ)
	$(CC) $(CFLAGS) -o $@ main.o $(OBJ) $(LDFLAGS)

interface: interface.o $(OBJ)
	$(CC) $(CFLAGS) -o $@ interface.o $(OBJ) $(LDFLAGS)

check-service.o: check-service.c check-service.h return.h
	$(CC) $(CFLAGS) -c $<

lsdb.o: lsdb.c lsdb.h return.h
	$(CC) $(CFLAGS) -c $<

return.o: return.c return.h
	$(CC) $(CFLAGS) -c $<

route.o: route.c route.h
	$(CC) $(CFLAGS) -c $<

exchanges.o: exchanges.c exchanges.h return.h
	$(CC) $(CFLAGS) -c $<

main.o: main.c check-service.h lsdb.h return.h route.h
	$(CC) $(CFLAGS) -c $<

interface.o: interface.c check-service.h lsdb.h return.h route.h exchanges.h
	$(CC) $(CFLAGS) -c $<

.PHONY: clean
clean:
	rm -f *.o $(BIN)
