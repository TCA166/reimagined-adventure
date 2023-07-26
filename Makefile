all: process daemon

monitor.o: monitor.c
	gcc monitor.c -c -o monitor.o $(CFLAGS)

process: process.c monitor.o
	gcc process.c monitor.o -o process $(CFLAGS) -lmagic

daemon: daemon.c monitor.o
	gcc daemon.c monitor.o -o daemon $(CFLAGS) -lmagic

clean:
	rm daemon
	rm process
	rm *.o