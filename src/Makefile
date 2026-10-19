CC = gcc
CFLAGS = -Wall -Wextra -g
LDFLAGS = -ljansson
OBJ = check-service.o lsdb.o return.o route.o exchanges.o log.o

BIN = main interface show

all: $(BIN)

main: main.o $(OBJ)
	$(CC) $(CFLAGS) -o $@ main.o $(OBJ) $(LDFLAGS)

interface: interface.o check-service.o lsdb.o return.o log.o
	$(CC) $(CFLAGS) -o $@ interface.o check-service.o lsdb.o return.o log.o $(LDFLAGS)

show: show.o lsdb.o return.o log.o
	$(CC) $(CFLAGS) -o $@ show.o lsdb.o return.o log.o $(LDFLAGS)

check-service.o: check-service.c check-service.h return.h lsdb.h
	$(CC) $(CFLAGS) -c $<

lsdb.o: lsdb.c lsdb.h return.h log.h
	$(CC) $(CFLAGS) -c $<

return.o: return.c return.h
	$(CC) $(CFLAGS) -c $<

route.o: route.c route.h lsdb.h return.h log.h
	$(CC) $(CFLAGS) -c $<

exchanges.o: exchanges.c exchanges.h lsdb.h return.h log.h
	$(CC) $(CFLAGS) -c $<

log.o: log.c log.h
	$(CC) $(CFLAGS) -c $<

main.o: main.c lsdb.h return.h route.h exchanges.h log.h
	$(CC) $(CFLAGS) -c $<

interface.o: interface.c check-service.h lsdb.h return.h log.h
	$(CC) $(CFLAGS) -c $<

show.o: show.c lsdb.h return.h
	$(CC) $(CFLAGS) -c $<

.PHONY: clean
clean:
	rm -f *.o $(BIN)
