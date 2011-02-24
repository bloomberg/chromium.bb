# Copyright 2011 The Native Client Authors.  All rights reserved.
# Use of this source code is governed by a BSD-style license that can
# be found in the LICENSE file.
#
# A simple (Linux) example of building a client of a .so without having to have
# that .so present.  main depends upon libsimple.so.  To create that dependency
# when linking main, we create a dummy library, libempty.so, which has
# soname=libsimple.so.
#
# This is intended to demonstrate how we might use ld on the platform until
# llc produces .so files directly.

CFLAGS=-Wall -fPIC

SIMPLE_OBJECTS=hello.o fortytwo.o

EMPTY_OBJECTS=empty.o

MAIN_OBJECTS=main.o
MAIN_LINK_FLAGS=-Wl,--unresolved-symbols=ignore-all

all: run

#libsimple is the real shared library.
libsimple.so: $(SIMPLE_OBJECTS)
	gcc -o $@ -shared -Wl,-soname,libsimple.so $(SIMPLE_OBJECTS)

# libempty.so simply embeds "soname=libsimple.so".  It is otherwise empty.
libempty.so: $(EMPTY_OBJECTS)
	ld -o $@ -shared -soname=libsimple.so --strip-all $(EMPTY_OBJECTS)

# main is made to depend on libsimple.so by linking with libempty.so
main: $(MAIN_OBJECTS) libempty.so
	gcc -o main $(MAIN_LINK_FLAGS) -L. -lempty $(MAIN_OBJECTS)

run: main libsimple.so
	LD_LIBRARY_PATH="." ./main

clean:
	rm main
	rm libsimple.so libempty.so
	rm $(SIMPLE_OBJECTS)
	rm $(EMPTY_OBJECTS)
	rm $(MAIN_OBJECTS)
