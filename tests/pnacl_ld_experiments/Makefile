# Copyright (c) 2012 The Native Client Authors.  All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# simple example(s) for builing shared images with the pnacl TC
# which do not require glibc

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
PATH_CRT=$(ROOT)/toolchain/pnacl_linux_x86_64/glibc/lib
PATH_CRT_SRC=$(ROOT)/pnacl/support

COMMON_FLAGS= --pnacl-driver-verbose -save-temps

RUNNER=$(BOOTER) $(LOADER) -S -E LD_DEBUG=all \
       --r_debug=0xXXXXXXXXXXXXXXXX -a -B $(IRT) \
       -- $(LDSO) --library-path $(LIBPATH_SHARED):.


######################################################################
PNACL_CC_FLAGS = $(COMMON_FLAGS) -I ../../..

hello1.bc:  hello1.c
	$(PNACL_CC)  -c $(PNACL_CC_FLAGS) -o $@ $^

fortytwo.bc:  fortytwo.c
	$(PNACL_CC)  -c $(PNACL_CC_FLAGS) -o $@ $^

crtbegin.bc:  $(PATH_CRT_SRC)/crtbegin.c
	$(PNACL_CC)  -c  $(PNACL_CC_FLAGS) -o $@ $^

crtend.bc:  $(PATH_CRT_SRC)/crtend.c
	$(PNACL_CC)  -c  $(PNACL_CC_FLAGS) -o $@ $^

startup_newlib_dynamic.bc: startup_newlib_dynamic.c
	$(PNACL_CC)  -c  $(PNACL_CC_FLAGS) -o $@ $^

nacl_irt.bc: $(ROOT)/src/untrusted/nacl/nacl_irt.c
	$(PNACL_CC)  -c  $(PNACL_CC_FLAGS) -o $@ $^

# misc stuff we expect to be available
prereqs:
	cd ../../ ; ./scons bitcode=1 platform=x86-32 \
          irt nacl_helper_bootstrap sel_ldr

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


LD_FLAGS_NATIVE = --pnacl-allow-native -arch x86-32

# bookends for dynamic images
# NOTE: the duplication of crtbegin.bc.
# The one here was build from $(PATH_CRT_SRC)/bitcode
# The one above from $(PATH_CRT_SRC)
INIT = crtbegin.bc startup_newlib_dynamic.bc nacl_irt.bc\
       $(PATH_CRT)/crti.bc $(PATH_CRT)/crtbegin.bc

FINI = crtend.bc

# we run this to steal the .o file
# the nexe produced by this is unfortunately not a dynamic image
# because of special handling of -l:ld-2.9.so which gets passed through as
# --add-extra-dt-needed=ld-nacl-x86-32.so.1 which does not trigger the
# a dynamic image
LIBS = -L $(LIBPATH_SHARED)  -l:ld-2.9.so  -L . -l:libsimple.pso -l:keep_alive.x
hello1.x86-32.nexe.dummy: hello1.bc $(INIT) $(FINI) libsimple.pso
	$(PNACL_CC) $(COMMON_FLAGS) $(LD_FLAGS_NATIVE) -nodefaultlibs -nostdlib \
        $(INIT) hello1.bc $(LIBS) $(FINI)  -o $@


# We steal this object file from hello1.x86-32.nexe.dummy
# NOTE:
# NOTE: the dependency tracking is broken here
# NOTE: you must run "make hello1.x86-32.nexe.dummy" which fails
# NOTE: followed by "hello1.x86-32.nexe"
# NOTE:
MAGIC_OBJ = hello1.x86-32.nexe.dummy---linked.o
LIBS2 = -L $(LIBPATH_SHARED)  -l:ld-2.9.so  -L . -l:libsimple.so

hello1.x86-32.nexe: libsimple.so
	$(PNACL_LD) $(COMMON_FLAGS) -arch x86-32 $(MAGIC_OBJ) $(LIBS2)  -o $@


######################################################################

run1: hello1.x86-32.nexe
	${RUNNER}  ./hello1.x86-32.nexe

######################################################################

clean:
	rm -f *.bc *.o *.ll *.so main *.pso *.raw *.nexe *.pexe *.meta
