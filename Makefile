CC      = /usr/bin/gcc
CFLAGS  = -Wall -O3 `pkg-config --cflags glib-2.0`
LDFLAGS = `pkg-config --libs glib-2.0 gio-2.0`
DEPENDFILE = .depend

SRC = nuimo.c example.c
OBJ = nuimo.o example.o
BIN = example

all:	example

debug:	CFLAGS += -DDEBUG -g
debug:	example

example:	$(OBJ)
	$(CC) $(CFLAGS) -o example $(OBJ) $(LDFLAGS)

%.o:	%.c
	$(CC) $(CFLAGS) -c $<

doc: doxygen_conf.dox $(OBJ) 
	doxygen doxygen_conf.dox

clean:
	rm -rf $(BIN) $(OBJ)

.PHONY:	clean all doc
