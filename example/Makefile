CPPFLAGS=-Wall -Wextra -g -O0 `pkg-config --cflags sdl3`
LDFLAGS=`pkg-config --libs sdl3`
OBJ=libvirtterm-example.o ../libvirtterm.o

all: libvirtterm-example

libvirtterm-example: ${OBJ}

clean:
	rm -f ${OBJ} libvirtterm-example
