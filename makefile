CC=gcc
IDIR = include
ODIR=obj
CFLAGS= -I$(IDIR) -lpthread -lrt -Wall -std=gnu99


DEPS = server.h client.h fileop.h table.h helper.h daemon.h
_DEPS = $(patsubst %,$(IDIR)/%,$(DEPS))

_OBJ = client.o fileop.o helper.o
OBJ = $(patsubst %,$(ODIR)/%,$(_OBJ))

_OBJ_SERVER = server.o table.o helper.o daemon.o
OBJ_SERVER = $(patsubst %,$(ODIR)/%,$(_OBJ_SERVER))

$(ODIR)/%.o: %.c $(_DEPS)
		$(CC) -c -o $@ $< $(CFLAGS)

all: client server

client: $(OBJ)
		$(CC) -o $@ $^ $(CFLAGS)

server: $(OBJ_SERVER)
		$(CC) -o $@ $^ $(CFLAGS)

.PHONY: clean
clean:
		rm -f $(ODIR)/*.o
		rm -f *.o
	