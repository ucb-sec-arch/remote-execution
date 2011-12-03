CFLAGS= -DDEBUG -DLOGGING -DUSE_TXT #-DUSE_ENCRYPTION
export CFLAGS

all:
	$(MAKE) -C client/
	$(MAKE) -C server/

clean:
	$(MAKE) clean -C client/
	$(MAKE) clean -C server/
