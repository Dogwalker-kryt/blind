CC      := gcc
VERSION := $(shell git describe --abbrev=4 --always --tags)
CFLAGS  += -DVERSION=\"$(VERSION)\"

SRC := blind.c
BIN := blind

all:
	$(CC) $(CFLAGS) -o $(BIN) $(SRC)

clean:
	$(RM) $(BIN)
