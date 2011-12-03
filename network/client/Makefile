INCLUDES=-I../common/

all: client_exec 

client_exec: client.c
	gcc $(INCLUDES) $(CFLAGS) $< -o $@

clean:
	rm -f client_exec client_flicker_result
