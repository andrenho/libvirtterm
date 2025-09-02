CPPFLAGS=-Wall -Wextra -std=c23 -g -O0
LDFLAGS=

all: libvirtterm-tests

tests.o: ../libvirtterm.c

libvirtterm-tests: tests.o
	gcc -o $@ $^

check: libvirtterm-tests
	./$^

check-leaks: libvirtterm-tests
	valgrind --leak-check=full --show-reachable=yes --track-origins=yes ./$^

clean:
	rm -f ${OBJ} libvirtterm-tests
.PHONY: clean