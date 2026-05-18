CC = gcc
CFLAGS = -Wall -Wextra -g
LDFLAGS = -ljansson
OBJ = check-service.o lsdb.o return.o route.o exchanges.o

BIN = main interface show

all: $(BIN)

main: main.o $(OBJ)
	$(CC) $(CFLAGS) -o $@ main.o $(OBJ) $(LDFLAGS)

interface: interface.o check-service.o lsdb.o return.o
	$(CC) $(CFLAGS) -o $@ interface.o check-service.o lsdb.o return.o $(LDFLAGS)

show: show.o
	$(CC) $(CFLAGS) -o $@ show.o $(LDFLAGS)

check-service.o: check-service.c check-service.h return.h
	$(CC) $(CFLAGS) -c $<

lsdb.o: lsdb.c lsdb.h return.h
	$(CC) $(CFLAGS) -c $<

return.o: return.c return.h
	$(CC) $(CFLAGS) -c $<

route.o: route.c route.h lsdb.h return.h
	$(CC) $(CFLAGS) -c $<

exchanges.o: exchanges.c exchanges.h lsdb.h return.h
	$(CC) $(CFLAGS) -c $<

main.o: main.c lsdb.h return.h route.h exchanges.h
	$(CC) $(CFLAGS) -c $<

interface.o: interface.c check-service.h lsdb.h return.h
	$(CC) $(CFLAGS) -c $<

show.o: show.c
	$(CC) $(CFLAGS) -c $<

.PHONY: clean
clean:
	rm -f *.o $(BIN)
