all: process daemon

process: process.c
	gcc process.c -o process $(CFLAGS) -lmagic

daemon: daemon.c
	gcc daemon.c -o daemon $(CFLAGS)