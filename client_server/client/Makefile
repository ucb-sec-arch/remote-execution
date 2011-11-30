INCLUDES=-I../common/
CFLAGS=-DDEBUG

all: client 

client: client.c
	gcc $(INCLUDES) $(CFLAGS) $< -o $@

clean:
	rm -f client client_flicker_result
