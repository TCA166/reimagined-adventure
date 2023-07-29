all: process daemon

monitor.o: monitor.c
	gcc monitor.c -c -o monitor.o $(CFLAGS)

process: process.c monitor.o
	gcc process.c monitor.o -o process $(CFLAGS) -lmagic

daemon: daemon.c monitor.c
	gcc monitor.c -c -o monitor.o $(CFLAGS) -DDAEMON
	gcc daemon.c monitor.o -o daemon $(CFLAGS) -lmagic

clean:
	rm daemon
	rm process
	rm *.o

deb: CFLAGS := -O3
deb: all
	mkdir -p deb/bin
	mkdir -p deb/etc/systemd/system
	mv process ./deb/bin/directory-monitor-process
	mv daemon ./deb/bin/directory-monitor-daemon
	cp directory-monitor.service ./deb/etc/systemd/system
	dpkg-deb --build --root-owner-group deb
	mv deb.deb directory-monitor_0.1_x86.deb
