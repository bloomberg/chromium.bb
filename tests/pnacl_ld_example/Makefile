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

ARCH ?= -m64

CFLAGS = -Wall -fPIC $(ARCH)
LIBFLAGS = -shared $(ARCH) -Wl,-soname,libsimple$(ARCH).so

SIMPLE_OBJECTS = hello$(ARCH).o fortytwo$(ARCH).o
EMPTY_OBJECTS = empty$(ARCH).o
MAIN_OBJECTS = main$(ARCH).o

all: run


######################################################################
# libempty$(ARCH).so simply embeds "soname=libsimple$(ARCH).so".
# It is otherwise empty.
libempty$(ARCH).so: $(EMPTY_OBJECTS)
	${CC} ${LIBFLAGS} -o $@ --strip-all $(EMPTY_OBJECTS)

empty$(ARCH).o: empty.c
	${CC} -c ${CFLAGS} -o $@ $<


######################################################################
#libsimple$(ARCH) is the real shared library.
libsimple$(ARCH).so: $(SIMPLE_OBJECTS)
	${CC} ${LIBFLAGS} -o $@ $(SIMPLE_OBJECTS)

fortytwo$(ARCH).o: fortytwo.c
	${CC} -c ${CFLAGS} -o $@ $<

hello$(ARCH).o: hello.c
	${CC} -c ${CFLAGS} -o $@ $<


######################################################################
# main is made to depend on libsimple.so by linking with libempty.so
MAIN_LD_FLAGS = -Wl,--unresolved-symbols=ignore-all ${CFLAGS}

main$(ARCH): $(MAIN_OBJECTS) libempty$(ARCH).so
	${CC} -o main$(ARCH) $(MAIN_LD_FLAGS) -L. -lempty$(ARCH) $(MAIN_OBJECTS)

main$(ARCH).o: main.c
	${CC} -c ${CFLAGS} -o $@ $<


######################################################################
# Run the requested architecture binary using libsimple$(ARCH).so
run: main$(ARCH) libsimple$(ARCH).so
	LD_LIBRARY_PATH="." ./main$(ARCH)


######################################################################
clean:
	rm -f main-* *.ll *.s *.o *.so *.pso *raw
