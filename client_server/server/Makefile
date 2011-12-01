INCLUDES=-I../common/
CFLAGS=-DDEBUG

all: server

server: server.c
	gcc $(INCLUDES) $(CFLAGS) $< -o $@

clean:
	rm -f server pal.bin server_flicker_result server_flicker_input
