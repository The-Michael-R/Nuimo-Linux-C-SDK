CC      = /usr/bin/gcc
CFLAGS  = -Wall -O3 `pkg-config --cflags glib-2.0`
LDFLAGS = `pkg-config --libs glib-2.0 gio-2.0`
DEPENDFILE = .depend

SRC = nuimo.c example.c
OBJ = nuimo.o example.o
BIN = example

example:	$(OBJ)
	$(CC) $(CFLAGS) -o example $(OBJ) $(LDFLAGS)

%.o:	%.c
	$(CC) $(CFLAGS) -c $<

.PHONY:	clean
clean:
	rm -rf $(BIN) $(OBJ)
