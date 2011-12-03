INCLUDES=-I../common/

all: server_exec

server_exec: server.c
	gcc $(INCLUDES) $(CFLAGS) $< -o $@

clean:
	rm -f server_exec pal.bin server_flicker_result server_flicker_input
