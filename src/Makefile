CC = gcc
CFLAGS = -Wall -Wextra -g
LDFLAGS = -ljansson
OBJ = check-service.o lsdb.o return.o route.o

all: main interface

main: main.o $(OBJ)
	$(CC) $(CFLAGS) -o main main.o $(OBJ) $(LDFLAGS)

interface: interface.o $(OBJ)
	$(CC) $(CFLAGS) -o interface interface.o $(OBJ) $(LDFLAGS)

check-service.o: check-service.c check-service.h return.h
	$(CC) $(CFLAGS) -c check-service.c

lsdb.o: lsdb.c lsdb.h return.h
	$(CC) $(CFLAGS) -c lsdb.c

return.o: return.c return.h
	$(CC) $(CFLAGS) -c return.c

route.o: route.c route.h
	$(CC) $(CFLAGS) -c route.c

main.o: main.c check-service.h lsdb.h return.h route.h
	$(CC) $(CFLAGS) -c main.c

interface.o: interface.c check-service.h lsdb.h return.h route.h
	$(CC) $(CFLAGS) -c interface.c

clean:
	rm -f *.o main interface