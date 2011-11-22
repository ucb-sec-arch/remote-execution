all: client server

client: client.c
	gcc client.c -o client

server: server.c
	gcc server.c -o server

clean:
	rm -f client server pal.bin server_flicker_result client_flicker_result
