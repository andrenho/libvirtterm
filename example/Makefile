CPPFLAGS=-Wall -Wextra -Wno-sign-compare -std=c23 -g -O0 $(shell pkg-config --cflags sdl3)
LDFLAGS=$(shell pkg-config --libs sdl3)
OBJ=../libvirtterm.o ../libvirtterm_pty.o

all: libvirtterm-example libvirtterm-example-events

libvirtterm-example: ${OBJ} libvirtterm-example.o

libvirtterm-example-events: ${OBJ} libvirtterm-example-events.o

clean:
	rm -f ${OBJ} libvirtterm-example libvirtterm-example-events *.o
