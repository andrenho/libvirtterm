CPPFLAGS=-Wall -Wextra -g -O0 $(shell pkg-config --cflags sdl3)
LDFLAGS=$(shell pkg-config --libs sdl3)
OBJ=libvirtterm-example.o ../libvirtterm.o

all: libvirtterm-example

libvirtterm-example: ${OBJ}

clean:
	rm -f ${OBJ} libvirtterm-example
