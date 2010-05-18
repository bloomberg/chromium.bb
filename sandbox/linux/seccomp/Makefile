# Copyright (c) 2010 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# This Makefile temporarily has been checked into the source tree so that
# we can run the tests. It will be replaced with a proper gyp file.

CFLAGS = -g -O0 -Wall -Werror -Wextra -Wno-missing-field-initializers         \
         -Wno-unused-parameter -I.
LDFLAGS = -g
CPPFLAGS =
MODS := allocator library debug maps x86_decode securemem sandbox     \
        syscall syscall_table trusted_thread trusted_process                  \
        access exit clone getpid gettid ioctl ipc madvise mmap mprotect       \
        munmap open sigaction sigprocmask socketcall stat
OBJS64 := $(shell echo ${MODS} | xargs -n 1 | sed -e 's/$$/.o64/')
OBJS32 := $(shell echo ${MODS} | xargs -n 1 | sed -e 's/$$/.o32/')
HEADERS:= $(shell for i in ${MODS}; do [ -r "$$i" ] && echo "$$i"; done)

.SUFFIXES: .o64 .o32

all: test

clean:
	-rm -f *.o *.o32 *.o64 tests/*.o32 tests/*.o.64
	-rm -f core core.* vgcore vgcore.* strace.log*
	-rm -f run_tests_32 run_tests_64
	-rm -f tests/test_syscalls.o64 tests/test_syscalls.o32
	-rm -f tests/test-list.h

test: run_tests_64 run_tests_32
	./run_tests_64
	./run_tests_32

# TODO: Track header file dependencies properly
tests/test_syscalls.o64 tests/test_syscalls.o32: tests/test-list.h

tests/test-list.h: tests/list_tests.py tests/test_syscalls.cc
	python tests/list_tests.py tests/test_syscalls.cc > $@

run_tests_64: $(OBJS64) tests/test_syscalls.o64 tests/test-list.h
	g++ -m64 tests/test_syscalls.o64 $(OBJS64) -lpthread -lutil -o $@
run_tests_32: $(OBJS32) tests/test_syscalls.o32 tests/test-list.h
	g++ -m32 tests/test_syscalls.o32 $(OBJS32) -lpthread -lutil -o $@

.cc.o: ${HEADERS}
	${CXX} ${CFLAGS} ${CPPFLAGS} -c -o $@ $<

.cc.o64: ${HEADERS}
	${CXX} ${CFLAGS} ${CPPFLAGS} -fPIC -c -o $@ $<

.c.o64: ${HEADERS}
	${CC} ${CFLAGS} ${CPPFLAGS} --std=gnu99 -fPIC -c -o $@ $<

.cc.o32: ${HEADERS}
	${CXX} ${CFLAGS} ${CPPFLAGS} -m32 -fPIC -c -o $@ $<

.c.o32: ${HEADERS}
	${CC} ${CFLAGS} ${CPPFLAGS} -m32 --std=gnu99 -fPIC -c -o $@ $<
