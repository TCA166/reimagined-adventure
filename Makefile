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

check: monitor.o
	mkdir -p test1/rec test2 test3 moveTo1 moveTo2
	checkmk tests/monitor.check > tests/monitorCheck.c
	gcc tests/monitorCheck.c monitor.o -lcheck -lmagic -lm -lsubunit -Wall -o tests/monitorCheck
	./tests/monitorCheck
	rm -rf test1 test2 test3 moveTo1 moveTo2

deb: CFLAGS := -O3
deb: all
	mkdir -p packages/deb/bin
	mkdir -p packages/deb/etc/systemd/system
	mv process ./packages/deb/bin/directory-monitor-process
	mv daemon ./packages/deb/bin/directory-monitor-daemon
	cp directory-monitor.service ./packages/deb/etc/systemd/system
	dpkg-deb --build --root-owner-group ./packages/deb
	mv deb.deb directory-monitor_0.1_x86.deb

rpm: CFLAGS := -O3
rpm: all
	mkdir -p packages/rpm/SOURCES packages/rpm/BUILD packages/rpm/BUILDROOT packages/rpm/RPMS packages/rpm/SRPMS
	mkdir directory-monitor-0.0.1
	mv process directory-monitor-0.0.1/directory-monitor-process
	mv daemon directory-monitor-0.0.1/directory-monitor-daemon
	cp directory-monitor.service directory-monitor-0.0.1
	tar cvzf ./packages/rpm/SOURCES/directory-monitor.tar.gz directory-monitor-0.0.1
	rm -r directory-monitor-0.0.1
	rpmbuild --define "_topdir `pwd`/packages/rpm" -bb ./packages/rpm/SPECS/directory-monitor.spec