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
PNACL_CC=$(TC_ROOT)/bin/pnacl-clang
PNACL_LD=$(TC_ROOT)/bin/pnacl-nativeld
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
PATH_CRT=$(ROOT)/toolchain/pnacl_linux_x86_64/glibc/lib
PATH_CRT_SRC=$(ROOT)/pnacl/support

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

# NOTE: mimicking the glibc stuff, these are identical for shared/dynamic
crtbegin.bc:  $(PATH_CRT_SRC)/crtbegin.c
	$(PNACL_CC)  -c  $(PNACL_CC_FLAGS) -o $@ $^

crtend.bc:  $(PATH_CRT_SRC)/crtend.c
	$(PNACL_CC)  -c  $(PNACL_CC_FLAGS) -o $@ $^

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
            irt nacl_helper_bootstrap sel_ldr libnosys libnacl libpthread pnacl_irt_shim

######################################################################
# bookends for shared libs
S_INIT = crtbegin.bc $(PATH_CRT)/crti.bc $(PATH_CRT)/crtbeginS.bc

S_FINI = crtend.bc

SHARED_FLAGS_X8632 = $(COMMON_FLAGS) -arch x86-32 -nostdlib -shared -fPIC -Wl,-Lx8632
SHARED_FLAGS_X8664 = $(COMMON_FLAGS) -arch x86-64 -nostdlib -shared -fPIC -Wl,-Lx8664
######################################################################
# NOTE: without -nostdlib a gratuitous libc dep is added
libsimple.pso: $(S_INIT) fortytwo.bc $(S_FINI)
	$(PNACL_CC) $(COMMON_FLAGS) -nostdlib -shared \
        $(S_INIT) fortytwo.bc $(S_FINI) \
        -Wl,--soname=libsimple.so -o $@

x8632/libsimple.so: libsimple.pso
	$(PNACL_TRANS) $(SHARED_FLAGS_X8632) -o $@ $^

x8664/libsimple.so: libsimple.pso
	$(PNACL_TRANS) $(SHARED_FLAGS_X8664) -o $@ $^

# build a shared version of libnosys from the bitcode archive
mylibnosys.pso: $(S_INIT) $(S_FINI)
	$(PNACL_CC) $(COMMON_FLAGS) -nostdlib -shared \
              $(S_INIT) \
              -Wl,--whole-archive \
              $(LIBPATH_BITCODE_SDK)/libnosys.a \
              -Wl,--no-whole-archive \
              $(S_FINI) \
              -Wl,--soname=mylibnosys.so -o $@

x8632/mylibnosys.so: mylibnosys.pso
	$(PNACL_TRANS) $(SHARED_FLAGS_X8632) -o $@ $^

x8664/mylibnosys.so: mylibnosys.pso
	$(PNACL_TRANS) $(SHARED_FLAGS_X8664) -o $@ $^

# build a shared version of libpthread from the bitcode archive
mylibpthread.pso: $(S_INIT) $(S_FINI)
	$(PNACL_CC) $(COMMON_FLAGS) -nostdlib -shared \
              $(S_INIT) \
              -Wl,--whole-archive \
              $(LIBPATH_BITCODE_SDK)/libpthread.a \
              -Wl,--no-whole-archive \
              $(S_FINI) \
              -Wl,--soname=mylibpthread.so -o $@

x8632/mylibpthread.so: mylibpthread.pso
	$(PNACL_TRANS) $(SHARED_FLAGS_X8632) -o $@ $^

x8664/mylibpthread.so: mylibpthread.pso
	$(PNACL_TRANS) $(SHARED_FLAGS_X8664) -o $@ $^

# build a shared version of newlib from the bitcode archive
# work around some hacks that introduced a common symbol in the
# disabled malloc/free portions
mylibc.pso: $(S_INIT)  $(S_FINI) mylibnosys.pso
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
	$(PNACL_CC) $(COMMON_FLAGS) -nostdlib -shared \
              $(S_INIT) /tmp/mylibc/*.o  $(S_FINI) \
              -Wl,--soname=mylibc.so -L . -l:mylibnosys.pso -o $@

# NOTE: we are requiring the final .so here - we do not want this!
#       Eventually, the meta info embedded in mylibc.pso
#       should make this obsolete
x8632/mylibc.so: mylibc.pso x8632/mylibnosys.so
	$(PNACL_TRANS) $(SHARED_FLAGS_X8632) -o $@ mylibc.pso

x8664/mylibc.so: mylibc.pso x8664/mylibnosys.so
	$(PNACL_TRANS) $(SHARED_FLAGS_X8664) -o $@ mylibc.pso

######################################################################
# bookends for dynamic images
# NOTE: the duplication of crtbegin.bc.
# The one here was build from $(PATH_CRT_SRC)/bitcode
# The one above from $(PATH_CRT_SRC)
INIT = crtbegin.bc startup_newlib_dynamic.bc \
       $(PATH_CRT)/crti.bc $(PATH_CRT)/crtbegin.bc

FINI = crtend.bc

LIBS = -l:keep_alive.x \
       -L . \
       -l:libsimple.pso \
       -l:mylibc.pso \
       -l:mylibpthread.pso \
       -l:ld.so.pso \
       -L $(LIBPATH_BITCODE_SDK) \
       -l:libnacl.a

hello1.pexe: hello1.bc $(INIT) $(FINI) \
             libsimple.pso mylibc.pso mylibnosys.pso mylibpthread.pso ld.so.pso
	$(PNACL_CC) $(COMMON_FLAGS) -nodefaultlibs -nostdlib \
        $(INIT) hello1.bc $(LIBS) $(FINI)  -o $@

# NOTE: the translator seems to pull in additional shared libs, e.g.:
# ld-nacl-x86-32.so.1, libgcc_s.so.1
x8632/hello1.nexe: hello1.pexe \
    x8632/libsimple.so x8632/mylibc.so x8632/mylibnosys.so x8632/mylibpthread.so
	$(PNACL_TRANS) $(COMMON_FLAGS) -rpath-link=x8632/ -arch x86-32  $^  -o $@

# there are some driver issue with the irt shim, so we disable it for now
x8664/hello1.nexe: hello1.pexe \
    x8664/libsimple.so x8664/mylibc.so x8664/mylibnosys.so x8664/mylibpthread.so
	$(PNACL_TRANS) $(COMMON_FLAGS) -rpath-link=x8664/ -arch x86-64  $^  -o $@ \
              --noirtshim

######################################################################

run1.x8632: x8632/hello1.nexe
	${RUNNER_X8632}  $^

run1.x8664: x8664/hello1.nexe
	${RUNNER_X8664}  $^

######################################################################

clean:
	rm -f *.bc *.ll *.pso *.po *.pexe *.meta x8632/* x8664/*
