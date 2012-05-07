# Copyright (c) 2012 The Native Client Authors.  All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# simple example(s) for builing shared images with the pnacl TC
# which do not require glibc
#
# How to use:
# -----------
# make prereq
# make hello1.x86-32.nexe.dummy  (this will fail with undefined symbols
# make run1

ROOT=$(shell readlink -f ../../)
TC_ROOT=$(ROOT)/toolchain/pnacl_linux_x86_64/glibc
PNACL_CC=$(TC_ROOT)/bin/pnacl-clang
PNACL_LD=$(TC_ROOT)/bin/pnacl-nativeld
PNACL_TRANS=$(TC_ROOT)/bin/pnacl-translate

BOOTER=$(ROOT)/scons-out/opt-linux-x86-32/staging/nacl_helper_bootstrap
LOADER=$(ROOT)/scons-out/opt-linux-x86-32/staging/sel_ldr
IRT=$(ROOT)/scons-out/nacl_irt-x86-32/staging/irt.nexe
LDSO=$(ROOT)/toolchain/pnacl_linux_x86_64/lib-x86-32/runnable-ld.so
LIBPATH_SHARED=$(ROOT)/toolchain/pnacl_linux_x86_64/lib-x86-32
LIBPATH_NEWLIB=$(ROOT)/toolchain/pnacl_linux_x86_64/newlib/usr/lib
LIBPATH_SDK=$(ROOT)/scons-out/nacl-x86-32-pnacl-pexe-clang/lib
PATH_CRT=$(ROOT)/toolchain/pnacl_linux_x86_64/glibc/lib
PATH_CRT_SRC=$(ROOT)/pnacl/support

COMMON_FLAGS= --pnacl-driver-verbose -save-temps
RUNNER_DEBUG= # -E LD_DEBUG=all

RUNNER=$(BOOTER) $(LOADER) -S $(RUNNER_DEBUG) \
       --r_debug=0xXXXXXXXXXXXXXXXX -a -B $(IRT) \
       -- $(LDSO) --library-path $(LIBPATH_SHARED):.


######################################################################
PNACL_CC_FLAGS = $(COMMON_FLAGS) -I ../../..

hello1.bc:  hello1.c
	$(PNACL_CC)  -c $(PNACL_CC_FLAGS) -o $@ $^

fortytwo.bc:  fortytwo.c
	$(PNACL_CC)  -c $(PNACL_CC_FLAGS) -o $@ $^

# NOTE: mimicking the glibc stuff, these are identical for shared/dynamic
crtbegin.bc:  $(PATH_CRT_SRC)/crtbegin.c
	$(PNACL_CC)  -c  $(PNACL_CC_FLAGS) -o $@ $^

crtend.bc:  $(PATH_CRT_SRC)/crtend.c
	$(PNACL_CC)  -c  $(PNACL_CC_FLAGS) -o $@ $^

startup_newlib_dynamic.bc: startup_newlib_dynamic.c
	$(PNACL_CC)  -c  $(PNACL_CC_FLAGS) -o $@ $^

# misc stuff we expect to be available
# NOTE: this is not quite complete and it may be necessary
#       to run pnac/build.sh sdk
prereqs:
	cd ../../ ; ./scons bitcode=1 platform=x86-32 \
          irt nacl_helper_bootstrap sel_ldr libnosys libnacl

# bookends for shared libs
S_INIT = crtbegin.bc $(PATH_CRT)/crti.bc $(PATH_CRT)/crtbeginS.bc

S_FINI = crtend.bc

# NOTE: without -nostdlib a gratuitous libc dep is added
libsimple.pso: $(S_INIT) fortytwo.bc $(S_FINI)
	$(PNACL_CC) $(COMMON_FLAGS) -nostdlib -shared -fPIC \
        $(S_INIT) fortytwo.bc $(S_FINI) \
        -Wl,--soname=libsimple.so -o $@

libsimple.so: libsimple.pso
	$(PNACL_TRANS) $(COMMON_FLAGS) -arch x86-32 -nostdlib -shared -fPIC -o $@ $^

# build a shared version of newlib from the bitcode archive
# work around some hacks that introduced a common symbol in the
# disabled malloc/free portions
mylibc.pso: $(S_INIT)  $(S_FINI)
	rm -rf /tmp/mylibc
	mkdir -p /tmp/mylibc
	cd /tmp/mylibc; \
           ar xv $(ROOT)/toolchain/pnacl_linux_x86_64/newlib/usr/lib/libc.a
	rm /tmp/mylibc/lib_a-callocr.o
	rm /tmp/mylibc/lib_a-calloc.o
	rm /tmp/mylibc/lib_a-cfreer.o
	rm /tmp/mylibc/lib_a-freer.o
	rm /tmp/mylibc/lib_a-mallocr.o
	rm /tmp/mylibc/lib_a-malignr.o
	rm /tmp/mylibc/lib_a-msizer.o
	rm /tmp/mylibc/lib_a-mallinfor.o
	rm /tmp/mylibc/lib_a-mallstatsr.o
	rm /tmp/mylibc/lib_a-malloptr.o
	rm /tmp/mylibc/lib_a-reallocr.o
	rm /tmp/mylibc/lib_a-vallocr.o
	rm /tmp/mylibc/lib_a-pvallocr.o
	$(PNACL_CC) $(COMMON_FLAGS) -nostdlib -shared -fPIC \
              $(S_INIT) /tmp/mylibc/*.o  $(S_FINI) \
        -Wl,--soname=mylibc.so -o $@

mylibc.so: mylibc.pso
	$(PNACL_TRANS) $(COMMON_FLAGS) -arch x86-32 -nostdlib -shared -fPIC -o $@ $^


LD_FLAGS_NATIVE = --pnacl-allow-native -arch x86-32

# bookends for dynamic images
# NOTE: the duplication of crtbegin.bc.
# The one here was build from $(PATH_CRT_SRC)/bitcode
# The one above from $(PATH_CRT_SRC)
INIT = crtbegin.bc startup_newlib_dynamic.bc \
       $(PATH_CRT)/crti.bc $(PATH_CRT)/crtbegin.bc

FINI = crtend.bc

# we run this to steal the .o file
# the nexe produced by this is unfortunately not a dynamic image
# because of special handling of -l:ld-2.9.so which gets passed through as
# --add-extra-dt-needed=ld-nacl-x86-32.so.1 which does not trigger the
# a dynamic image
LIBS = -L $(LIBPATH_SHARED)  -l:ld-2.9.so  \
       -L . -l:libsimple.pso -l:keep_alive.x -l:mylibc.pso \
       -Wl,--start-group \
       -L $(LIBPATH_SDK) -l:libnacl.a -l:libnosys.a \
       -Wl,--end-group


hello1.x86-32.nexe.dummy: hello1.bc $(INIT) $(FINI) libsimple.pso mylibc.pso
	$(PNACL_CC) $(COMMON_FLAGS) $(LD_FLAGS_NATIVE) -nodefaultlibs -nostdlib \
        $(INIT) hello1.bc $(LIBS) $(FINI)  -o $@


# We steal this object file from hello1.x86-32.nexe.dummy
# NOTE:
# NOTE: the dependency tracking is broken here
# NOTE: you must run "make hello1.x86-32.nexe.dummy" which fails
# NOTE: followed by "hello1.x86-32.nexe"
# NOTE:
MAGIC_OBJ = hello1.x86-32.nexe.dummy---linked.o
LIBS2 = -L $(LIBPATH_SHARED)  -l:ld-2.9.so  \
        -L . -l:libsimple.so -l:mylibc.so \
        -l:libgcc.a

hello1.x86-32.nexe: libsimple.so mylibc.so
	$(PNACL_LD) $(COMMON_FLAGS) -arch x86-32 $(MAGIC_OBJ) $(LIBS2)  -o $@


######################################################################

run1: hello1.x86-32.nexe
	${RUNNER}  ./hello1.x86-32.nexe

######################################################################

clean:
	rm -f *.bc *.o *.ll *.so main *.pso *.raw *.nexe *.pexe *.meta
