all:
	make -C src
	cp src/port_test port_test
clean:
	make -C src clean
