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
LDSO=$(ROOT)/toolchain/pnacl_linux_x86_64/lib-x86-32/runnable-ld.so
LIBPATH_SHARED=$(ROOT)/toolchain/pnacl_linux_x86_64/lib-x86-32
PATH_CRT=$(ROOT)/toolchain/pnacl_linux_x86_64/glibc/lib
PATH_CRT_SRC=$(ROOT)/pnacl/support

COMMON_FLAGS= --pnacl-driver-verbose -save-temps

RUNNER=$(BOOTER) $(LOADER) -S -E LD_DEBUG=all --r_debug=0xXXXXXXXXXXXXXXXX -a \
       -- $(LDSO) --library-path $(LIBPATH_SHARED):.


######################################################################
PNACL_CC_FLAGS = $(COMMON_FLAGS)

hello1.bc:  hello1.c
	$(PNACL_CC)  -c $(PNACL_CC_FLAGS) -o $@ $^

fortytwo.bc:  fortytwo.c
	$(PNACL_CC)  -c $(PNACL_CC_FLAGS) -o $@ $^

crtbegin.bc:  $(PATH_CRT_SRC)/crtbegin.c
	$(PNACL_CC)  -c  $(PNACL_CC_FLAGS) -o $@ $^

crtend.bc:  $(PATH_CRT_SRC)/crtend.c
	$(PNACL_CC)  -c  $(PNACL_CC_FLAGS) -o $@ $^

# NOTE: without -nostdlib a gratuitous libc dep is added
#       but with -nostdlib we get:
#       error: symbol lookup error: undefined symbol: fortytwo (fatal)
libsimple.pso: fortytwo.bc
	$(PNACL_CC) $(COMMON_FLAGS) -Wl,--soname=libsimple.so -shared -fPIC -o $@ $^

libsimple.so: libsimple.pso
	$(PNACL_TRANS) -arch x86-32 -shared -fPIC -o $@ $^




LD_FLAGS_NATIVE = --pnacl-allow-native -arch x86-32
# NOTE: the duplication of crtbegin.bc.
# The one here was build from $(PATH_CRT_SRC)/bitcode
# The one above from $(PATH_CRT_SRC)
INIT = crtbegin.bc $(PATH_CRT)/crt1.bc $(PATH_CRT)/crti.bc \
       $(PATH_CRT)/crtdummy.bc $(PATH_CRT)/crtbegin.bc

FINI = crtend.bc

# we run this to steal the .o file
# the nexe produced by this is unfortunately not a dynamic image
# because of special handling of -l:ld-2.9.so which gets passed through as
# --add-extra-dt-needed=ld-nacl-x86-32.so.1 which does not trigger the
# a dynamic image
LIBS = -L $(LIBPATH_SHARED)  -l:ld-2.9.so  -L . -l:libsimple.pso
hello1.x86-32.nexe.dummy: hello1.bc crtend.bc crtbegin.bc libsimple.pso
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
	rm -f *.bc *.o *.ll *.so main *.pso *.raw *.nexe
