TARGETS=client server

all: $(TARGETS)

client: client.c
	gcc $< -o $@ -DDEBUG

server: server.c
	gcc $< -o $@ -DDEBUG

clean:
	rm -f client server pal.bin server_flicker_result client_flicker_result
