CC = gcc
CFLAGS = -Wall -Wextra -g
OBJ = check-service.o lsdb.o

all: main interface

main: main.o $(OBJ)
	$(CC) $(CFLAGS) -o main main.o $(OBJ)

interface: interface.o $(OBJ)
	$(CC) $(CFLAGS) -o interface interface.o $(OBJ)

check-service.o: check-service.c check-service.h
	$(CC) $(CFLAGS) -c check-service.c

lsdb.o: lsdb.c LSDB.h
	$(CC) $(CFLAGS) -c lsdb.c

main.o: main.c check-service.h LSDB.h
	$(CC) $(CFLAGS) -c main.c

interface.o: interface.c check-service.h LSDB.h
	$(CC) $(CFLAGS) -c interface.c

clean:
	rm -f *.o main interface
