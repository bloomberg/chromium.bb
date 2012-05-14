# Copyright (c) 2012 The Native Client Authors.  All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# simple example(s) for builing shared images with the pnacl TC
# which do not require glibc
#
# How to use:
# -----------
# make prereqs.x8632
# make run1.x8632

ROOT=$(shell readlink -f ../../)
TC_ROOT=$(ROOT)/toolchain/pnacl_linux_x86_64/glibc
PNACL_CC=$(TC_ROOT)/bin/pnacl-clang -Wall -Werror
PNACL_LD=$(TC_ROOT)/bin/pnacl-nativeld
PNACL_AR=$(TC_ROOT)/bin/pnacl-ar
PNACL_TRANS=$(TC_ROOT)/bin/pnacl-translate

BOOTER_X8632=$(ROOT)/scons-out/opt-linux-x86-32/staging/nacl_helper_bootstrap
BOOTER_X8664=$(ROOT)/scons-out/opt-linux-x86-64/staging/nacl_helper_bootstrap

LOADER_X8632=$(ROOT)/scons-out/opt-linux-x86-32/staging/sel_ldr
LOADER_X8664=$(ROOT)/scons-out/opt-linux-x86-64/staging/sel_ldr

IRT_X8632=$(ROOT)/scons-out/nacl_irt-x86-32/staging/irt.nexe
IRT_X8664=$(ROOT)/scons-out/nacl_irt-x86-64/staging/irt.nexe

LDSO_X8632=$(ROOT)/toolchain/pnacl_linux_x86_64/lib-x86-32/runnable-ld.so
LDSO_X8664=$(ROOT)/toolchain/pnacl_linux_x86_64/lib-x86-64/runnable-ld.so

LIBPATH_SHARED_X8632=$(ROOT)/toolchain/pnacl_linux_x86_64/lib-x86-32
LIBPATH_SHARED_X8664=$(ROOT)/toolchain/pnacl_linux_x86_64/lib-x86-64

# These looks like they have arch bias but we are just pulling bitcode
LIBPATH_NEWLIB=$(ROOT)/toolchain/pnacl_linux_x86_64/newlib/usr/lib
LIBPATH_BITCODE_SDK=$(ROOT)/scons-out/nacl-x86-32-pnacl-pexe-clang/lib
PATH_CRT_BITCODE=$(ROOT)/toolchain/pnacl_linux_x86_64/newlib/lib
PATH_CRT_X8632=$(ROOT)/toolchain/pnacl_linux_x86_64/lib-x86-32
PATH_CRT_X8664=$(ROOT)/toolchain/pnacl_linux_x86_64/lib-x86-64

COMMON_FLAGS= --pnacl-driver-verbose -save-temps
RUNNER_DEBUG= # -E LD_DEBUG=all

RUNNER_X8632=$(BOOTER_X8632) $(LOADER_X8632) -S $(RUNNER_DEBUG) \
       --r_debug=0xXXXXXXXXXXXXXXXX -a -B $(IRT_X8632) \
       -- $(LDSO_X8632) --library-path $(LIBPATH_SHARED_X8632):x8632

RUNNER_X8664=$(BOOTER_X8664) $(LOADER_X8664) -S $(RUNNER_DEBUG) \
       --r_debug=0xXXXXXXXXXXXXXXXX -a -B $(IRT_X8664) \
       -- $(LDSO_X8664) --library-path $(LIBPATH_SHARED_X8664):x8664


######################################################################
PNACL_CC_FLAGS = $(COMMON_FLAGS) -I ../../..

hello1.bc:  hello1.c
	$(PNACL_CC)  -c $(PNACL_CC_FLAGS) -o $@ $^

fortytwo.bc:  fortytwo.c
	$(PNACL_CC)  -c $(PNACL_CC_FLAGS) -o $@ $^

startup_newlib_dynamic.bc: startup_newlib_dynamic.c
	$(PNACL_CC)  -c  $(PNACL_CC_FLAGS) -o $@ $^

######################################################################
# misc stuff we expect to be available
# NOTE: this is not quite complete and it may be necessary
#       to run pnac/build.sh sdk

# NOTE: the soname has an arch bias
# we need to fix this by having all ld.so advertise a neutral soname
ld.so.pso: ld.so.dummy.c
	$(PNACL_CC) $(COMMON_FLAGS) -nostdlib -shared \
        -Wl,--soname=ld-nacl-x86-32.so.1 $^ -o $@

prereqs.x8632: ld.so.pso
	mkdir -p x8632
	cd ../../ ; ./scons bitcode=1 platform=x86-32 -j 8 \
            irt nacl_helper_bootstrap sel_ldr libnosys libnacl libpthread

prereqs.x8664: ld.so.pso
	mkdir -p x8664
	cd ../../ ; ./scons bitcode=1 platform=x86-64 -j 8 \
            irt nacl_helper_bootstrap sel_ldr libnosys libnacl \
            libpthread pnacl_irt_shim

######################################################################
# bookends for shared libs
S_INIT_BITCODE = $(PATH_CRT_BITCODE)/crti.bc $(PATH_CRT_BITCODE)/crtbeginS.bc
S_FINI_BITCODE =

S_INIT_X8632 = $(PATH_CRT_X8632)/crtbeginS.o
S_FINI_X8632 = $(PATH_CRT_X8632)/crtendS.o

S_INIT_X8664 = $(PATH_CRT_X8664)/crtbeginS.o
S_FINI_X8664 = $(PATH_CRT_X8664)/crtendS.o

SHARED_FLAGS_X8632 = $(COMMON_FLAGS) -arch x86-32 -nostdlib -shared -fPIC -Wl,-Lx8632
SHARED_FLAGS_X8664 = $(COMMON_FLAGS) -arch x86-64 -nostdlib -shared -fPIC -Wl,-Lx8664
######################################################################
# NOTE: without -nostdlib a gratuitous libc dep is added
libsimple.pso: fortytwo.bc
	$(PNACL_CC) $(COMMON_FLAGS) -nostdlib -shared \
        $(S_INIT_BITCODE) fortytwo.bc $(S_FINI_BITCODE) \
        -Wl,--soname=libsimple.so -o $@

x8632/libsimple.so: libsimple.pso
	$(PNACL_TRANS) $(SHARED_FLAGS_X8632) -o $@ \
            $(S_INIT_X8632) $^ $(S_FINI_X8632)

x8664/libsimple.so: libsimple.pso
	$(PNACL_TRANS) $(SHARED_FLAGS_X8664) -o $@ \
            $(S_INIT_X8664) $^ $(S_FINI_X8664)
# build a shared version of libnosys from the bitcode archive
mylibnosys.pso: $(LIBPATH_BITCODE_SDK)/libnosys.a
	$(PNACL_CC) $(COMMON_FLAGS) -nostdlib -shared \
              $(S_INIT_BITCODE) \
              -Wl,--whole-archive \
              $(LIBPATH_BITCODE_SDK)/libnosys.a \
              -Wl,--no-whole-archive \
              $(S_FINI_BITCODE) \
              -Wl,--soname=mylibnosys.so -o $@

x8632/mylibnosys.so: mylibnosys.pso
	$(PNACL_TRANS) $(SHARED_FLAGS_X8632) -o $@ \
            $(S_INIT_X8632) $^ $(S_FINI_X8632)

x8664/mylibnosys.so: mylibnosys.pso
	$(PNACL_TRANS) $(SHARED_FLAGS_X8664) -o $@ \
            $(S_INIT_X8664) $^ $(S_FINI_X8664)

# build a shared version of libpthread from the bitcode archive
mylibpthread.pso: $(LIBPATH_BITCODE_SDK)/libpthread.a
	$(PNACL_CC) $(COMMON_FLAGS) -nostdlib -shared \
              $(S_INIT_BITCODE) \
              -Wl,--whole-archive \
              $(LIBPATH_BITCODE_SDK)/libpthread.a \
              -Wl,--no-whole-archive \
              $(S_FINI_BITCODE) \
              -Wl,--soname=mylibpthread.so -o $@

x8632/mylibpthread.so: mylibpthread.pso
	$(PNACL_TRANS) $(SHARED_FLAGS_X8632) -o $@ \
            $(S_INIT_X8632) $^ $(S_FINI_X8632)

x8664/mylibpthread.so: mylibpthread.pso
	$(PNACL_TRANS) $(SHARED_FLAGS_X8664) -o $@ \
            $(S_INIT_X8664) $^ $(S_FINI_X8664)
# build a shared version of newlib from the bitcode archive
# work around some hacks that introduced a common symbol in the
# disabled malloc/free portions
mylibc.pso: mylibnosys.pso \
            $(ROOT)/toolchain/pnacl_linux_x86_64/newlib/usr/lib/libc.a
	$(PNACL_CC) $(COMMON_FLAGS) -nostdlib -shared \
              $(S_INIT_BITCODE) \
              -Wl,--whole-archive \
              $(ROOT)/toolchain/pnacl_linux_x86_64/newlib/usr/lib/libc.a \
              -Wl,--no-whole-archive \
              $(S_FINI_BITCODE) \
              -Wl,--soname=mylibc.so -L . -l:mylibnosys.pso -o $@

# NOTE: we are requiring the final .so here - we do not want this!
#       Eventually, the meta info embedded in mylibc.pso
#       should make this obsolete
x8632/mylibc.so: mylibc.pso x8632/mylibnosys.so
	$(PNACL_TRANS) $(SHARED_FLAGS_X8632) -o $@ \
            $(S_INIT_X8632) mylibc.pso $(S_FINI_X8632)

x8664/mylibc.so: mylibc.pso x8664/mylibnosys.so
	$(PNACL_TRANS) $(SHARED_FLAGS_X8664) -o $@ \
            $(S_INIT_X8664) mylibc.pso $(S_FINI_X8664)
######################################################################
mylibnacl.a: $(LIBPATH_BITCODE_SDK)/libnacl.a
	rm -f $@
	rm -rf /tmp/libnacl
	mkdir -p /tmp/libnacl
	cd /tmp/libnacl; ar xv $^
	rm /tmp/libnacl/pthread_initialize_minimal.bc
	rm /tmp/libnacl/pthread_stubs.bc
	rm /tmp/libnacl/start.bc
	rm /tmp/libnacl/tls.bc
	$(PNACL_AR) cr $@ /tmp/libnacl/*
######################################################################
# bookends for dynamic images
# NOTE: the duplication of crtbegin.bc.
# The one here was build from $(PATH_CRT_SRC)/bitcode
# The one above from $(PATH_CRT_SRC)
INIT_BITCODE = startup_newlib_dynamic.bc \
       $(PATH_CRT_BITCODE)/crti.bc $(PATH_CRT_BITCODE)/crtbegin.bc
FINI_BITCODE =

INIT_X8632 = $(PATH_CRT_X8632)/crtbegin.o
FINI_X8632 = $(PATH_CRT_X8632)/crtend.o


INIT_X8664 = $(PATH_CRT_X8664)/crtbegin.o \
             $(ROOT)/scons-out/nacl-x86-64-pnacl-pexe-clang/lib/libpnacl_irt_shim.a
FINI_X8664 = $(PATH_CRT_X8664)/crtend.o

LIBS = -l:keep_alive.x \
       -L . \
       -l:libsimple.pso \
       -l:mylibc.pso \
       -l:mylibpthread.pso \
       -l:ld.so.pso \
       -l:mylibnacl.a

# NOTE: it is not clear why we have to specify ld-2.9.so here
#       which should be the same as the loader.
#       Without it ___tls_get_addr is not found
#
#       Note: we are adding the shim manually
DYNAMIC_FLAGS =  -nodefaultlibs -Wl,-l:libgcc.a  -Wl,-l:libcrt_platform.a
DYNAMIC_FLAGS_X8632 = $(DYNAMIC_FLAGS)  -rpath-link=x8632 -arch x86-32 \
                      $(LIBPATH_SHARED_X8632)/ld-2.9.so
DYNAMIC_FLAGS_X8664 = $(DYNAMIC_FLAGS)  -rpath-link=x8664/ -arch x86-64 \
                      --noirtshim -Wl,--entry=_pnacl_wrapper_start \
                      $(LIBPATH_SHARED_X8664)/ld-2.9.so

hello1.pexe: hello1.bc startup_newlib_dynamic.bc ld.so.pso libsimple.pso \
             mylibc.pso mylibnosys.pso mylibpthread.pso mylibnacl.a
	$(PNACL_CC) $(COMMON_FLAGS) -nodefaultlibs -nostdlib \
        $(INIT_BITCODE) hello1.bc $(LIBS) $(FINI_BITCODE)  -o $@

x8632/hello1.nexe: hello1.pexe \
    x8632/libsimple.so x8632/mylibc.so x8632/mylibnosys.so x8632/mylibpthread.so
	$(PNACL_TRANS) $(COMMON_FLAGS) $(DYNAMIC_FLAGS_X8632)  -o $@ \
            $(INIT_X8632) $^ $(FINI_X8632)
# We are sneaking in the shim manually via $(INIT_X8664) for now
x8664/hello1.nexe: hello1.pexe \
    x8664/libsimple.so x8664/mylibc.so x8664/mylibnosys.so x8664/mylibpthread.so
	$(PNACL_TRANS) $(COMMON_FLAGS) $(DYNAMIC_FLAGS_X8664)  -o $@ \
            $(INIT_X8664) $^ $(FINI_X8664)

######################################################################

run1.x8632: x8632/hello1.nexe
	${RUNNER_X8632}  $^

run1.x8664: x8664/hello1.nexe
	${RUNNER_X8664}  $^

######################################################################

clean:
	rm -f *.bc *.ll *.pso *.po *.a *.pexe *.meta x8632/* x8664/*
