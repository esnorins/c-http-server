CC := gcc
CFLAGS := -Os -ggdb -Wall -Wextra -Wpedantic

all: server

clean: server
	rm $^

server: main.c
	$(CC) -o $@ $^ $(CFLAGS)
