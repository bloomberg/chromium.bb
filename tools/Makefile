 # Copyright 2008, Google Inc.
# All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are
# met:
#
#     * Redistributions of source code must retain the above copyright
# notice, this list of conditions and the following disclaimer.
#     * Redistributions in binary form must reproduce the above
# copyright notice, this list of conditions and the following disclaimer
# in the documentation and/or other materials provided with the
# distribution.
#     * Neither the name of Google Inc. nor the names of its
# contributors may be used to endorse or promote products derived from
# this software without specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
# "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
# LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
# A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
# OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
# SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
# LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
# DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
# THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
# OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

##################################################################
#  Description: Main makefile for NativeClient toolchain.
#  This is currently incomplete, because it does not build app_lib
#  and move it to the proper location.
#  TODO(sehr): Consolidate with app_lib build.
##################################################################

default: build

##################################################################
# Describe where to find the sources, where the SDK goes, etc.
# SDKROOT has to exist, which requires some extra work, especially
# on Linux.
# TODO(sehr): Figure out how to get SDKROOT not to be absolute
#             while keeping prefix set correctly.
##################################################################

THISMAKEFILE := $(lastword $(MAKEFILE_LIST))

CROSSARCH=nacl
SDKLOC?=/usr/local
SDKNAME=nacl-sdk
SDKROOT=$(SDKLOC)/$(SDKNAME)
BUILDNCCDIR=$(SDKROOT)/$(CROSSARCH)

ifeq ($(shell uname -s), Darwin)
 PLATFORM=osx
else
 # No 'uname -o' on OSX
 PLATFORM:=$(shell uname -o)
endif

SRCDIR=../../../third_party

ifeq ($(PLATFORM), Cygwin)
  # Ugh, Cygwin and spaces in paths don't work well.
  # I'm explicitly coding the path.
  BUILDPATH=$(SDKROOT)/bin:/usr/local/bin:/usr/bin:/bin:/cygdrive/c/Python25/:/usr/X11R6/bin:/cygdrive/c/WINDOWS/system32:/cygdrive/c/WINDOWS:/cygdrive/c/WINDOWS/system32/WBEM:/cygdrive/c/WINDOWS/system32/WindowsPowerShell/v1.0:/usr/lib/lapack
  SCONS?=scons.bat
  SDKLOC_NATIVE=$(shell cygpath -m $(SDKLOC))
  CYGWIN_DLL_COPY= \
    cp -f /cygdrive/c/cygwin/bin/cygwin1.dll \
    /cygdrive/c/cygwin/bin/cygiconv*.dll \
    /cygdrive/c/cygwin/bin/cygintl*.dll \
    $(SDKROOT)/bin
else
  BUILDPATH=$(SDKROOT)/bin:$(PATH)
  SCONS?=scons
  SDKLOC_NATIVE=$(SDKLOC)
  CYGWIN_DLL_COPY=echo "Cygwin not used on this platform"
endif

##################################################################
#  The default alignment used by the tools.
#  DEFAULT_ALIGN_BYTES is the alignment constraint that is enforced
#  by the tools.  No instruction may span a multiple of this value.
#  DEFAULT_ALIGN_POW2 is the log base 2 of DEFAULT_ALIGN_BYTES.
#  Both of these variables are used to set the default values of
#  alignment for gcc and gas.  Command-line flags may override the
#  values.
##################################################################
DEFAULT_ALIGN_BYTES=32
DEFAULT_ALIGN_POW2=5

##################################################################
#  The version numbers for the tools we will be building.
##################################################################
BINUTILS_VERSION=2.18
NEWLIB_VERSION=1.17.0
GCC_VERSION=4.2.2
GDB_VERSION=6.8

##################################################################
# Copying the sources, applying patches, etc.
##################################################################
# Create the BUILD subdirectory:
BUILD:
	mkdir BUILD

# copy binutils, apply patches
BUILD/binutils-$(BINUTILS_VERSION): | BUILD
	@echo "Unpacking binutils-$(BINUTILS_VERSION)"
	# binutils_$(BINUTILS_VERSION).orig.tar.gz is debian-style
	cd BUILD && \
	if [ -e "$(SRCDIR)/binutils/binutils_$(BINUTILS_VERSION).orig.tar.gz" ] ; then \
	  tar xf $(SRCDIR)/binutils/binutils_$(BINUTILS_VERSION).orig.tar.gz ; \
	else \
	  tar xf $(SRCDIR)/binutils/binutils-$(BINUTILS_VERSION).tar.bz2 ; \
	fi
	cd BUILD && patch -p0 < ../patches/binutils-$(BINUTILS_VERSION).patch
	if ( ! [ -x /usr/bin/bison ] ) || ( ! [ -x /usr/bin/flex ] ) ; then \
	  cd BUILD && ( patch -p0 < ../patches/binutils-$(BINUTILS_VERSION).ly.patch || true ) ; \
	fi

# create binutils source directory, and mark that we've done that
BUILD/stamp-binutils-$(BINUTILS_VERSION): Makefile
	rm -rf BUILD/binutils-$(BINUTILS_VERSION)
	$(MAKE) -f $(THISMAKEFILE) BUILD/binutils-$(BINUTILS_VERSION)
	touch $@

# copy newlib, apply patches
BUILD/newlib-$(NEWLIB_VERSION): | BUILD
	@echo "Unpacking newlib-$(NEWLIB_VERSION)"
	cd BUILD && tar xf $(SRCDIR)/newlib/newlib-$(NEWLIB_VERSION).tar.gz
	cd BUILD && patch -p0 < ../patches/newlib-$(NEWLIB_VERSION).patch

# create newlib source directory, and mark that we've done that
BUILD/stamp-newlib-$(NEWLIB_VERSION): Makefile
	rm -rf BUILD/newlib-$(NEWLIB_VERSION)
	$(MAKE) -f $(THISMAKEFILE) BUILD/newlib-$(NEWLIB_VERSION)
	touch $@

# copy gcc, apply patches
BUILD/gcc-$(GCC_VERSION): | BUILD
	@echo "Unpacking gcc-$(GCC_VERSION)"
	cd BUILD && tar xf $(SRCDIR)/gcc/gcc-core-$(GCC_VERSION).tar.bz2
	cd BUILD && tar xf $(SRCDIR)/gcc/gcc-g++-$(GCC_VERSION).tar.bz2
	cd BUILD && patch -p0 < ../patches/gcc-$(GCC_VERSION).patch

# create gcc source directory, and mark that we've done that
BUILD/stamp-gcc-$(GCC_VERSION): Makefile
	rm -rf BUILD/gcc-$(GCC_VERSION)
	$(MAKE) -f $(THISMAKEFILE) BUILD/gcc-$(GCC_VERSION)
	touch $@

# copy gdb, apply patches
BUILD/gdb-$(GDB_VERSION): | BUILD
	@echo "Unpacking gdb-$(GDB_VERSION)"
	cd BUILD && tar xf $(SRCDIR)/gdb/gdb-$(GDB_VERSION).tar.bz2
	cd BUILD && patch -p0 < ../patches/gdb-$(GDB_VERSION).patch

# create gdb source directory, and mark that we've done that
BUILD/stamp-gdb-$(GDB_VERSION): #Makefile
	rm -rf BUILD/gdb-$(GDB_VERSION)
	$(MAKE) -f $(THISMAKEFILE) BUILD/gdb-$(GDB_VERSION)
	touch $@

##################################################################
# Create the SDK output directories.
##################################################################
SDKDIRS=\
    $(SDKROOT)/bin \
    $(SDKROOT)/lib \
    $(SDKROOT)/include \
    $(SDKROOT)/$(CROSSARCH)/include \
    $(SDKROOT)/$(CROSSARCH)/lib \

.PHONY: sdkdirs
sdkdirs:
	echo "Creating the SDK tree at $(SDKROOT)"
	@for d in $(SDKDIRS); do \
	  if [ ! -d $$d ]; then \
	    echo install -m 755 -d $$d; \
	    install -m 755 -d $$d; \
	  fi; \
	done

##################################################################
# binutils:
# Builds the cross assembler, linker, archiver, etc.
##################################################################
BUILD/stamp-binutils: BUILD/stamp-binutils-$(BINUTILS_VERSION) Makefile
	rm -rf BUILD/build-binutils-$(CROSSARCH)
	mkdir BUILD/build-binutils-$(CROSSARCH)
	cd BUILD/build-binutils-$(CROSSARCH) && \
	  CC="gcc" \
	  CFLAGS="-DNACL_ALIGN_BYTES=$(DEFAULT_ALIGN_BYTES) -DNACL_ALIGN_POW2=$(DEFAULT_ALIGN_POW2)" \
	  LDFLAGS="-s" \
	  ../binutils-$(BINUTILS_VERSION)/configure \
	    --prefix=$(SDKROOT) \
	    --target=${CROSSARCH}
	$(MAKE) -C BUILD/build-binutils-$(CROSSARCH) all
	$(MAKE) -C BUILD/build-binutils-$(CROSSARCH) install
	touch $@

.PHONY: binutils
binutils: BUILD/stamp-binutils

##################################################################
# pregcc:
# Builds the cross gcc used to build the libraries.
##################################################################
BUILD/stamp-pregcc: BUILD/stamp-gcc-$(GCC_VERSION) Makefile
	rm -rf BUILD/build-pregcc-$(CROSSARCH)
	mkdir BUILD/build-pregcc-$(CROSSARCH)
	cd BUILD/build-pregcc-$(CROSSARCH) && \
	  PATH=$(BUILDPATH) CC="gcc" \
		CFLAGS="-Dinhibit_libc -D__gthr_posix_h -DNACL_ALIGN_BYTES=$(DEFAULT_ALIGN_BYTES) -DNACL_ALIGN_POW2=$(DEFAULT_ALIGN_POW2)" \
	  LDFLAGS="-s" \
	    ../gcc-$(GCC_VERSION)/configure \
	      --prefix=$(SDKROOT) \
	      --without-headers \
	      --disable-libmudflap \
              --disable-decimal-float \
	      --disable-libssp \
	      --enable-languages=c \
	      --disable-threads \
	      --disable-libstdcxx-pch \
	      --disable-shared \
	      --target=$(CROSSARCH)
	if [ "x$(GCC_VERSION)" = "x4.4.1" ] ; then \
	PATH=$(BUILDPATH) \
	  $(MAKE) -C BUILD/build-pregcc-$(CROSSARCH) \
		  CFLAGS_FOR_TARGET="-O2 -g" \
		  CXXFLAGS_FOR_TARGET="-O2 -g" \
	    all-gcc all-target-libgcc ; \
	else \
	PATH=$(BUILDPATH) \
	  $(MAKE) -C BUILD/build-pregcc-$(CROSSARCH) \
		  CFLAGS_FOR_TARGET="-O2 -g" \
		  CXXFLAGS_FOR_TARGET="-O2 -g" \
	    all-gcc ; \
	fi
	if [ "x$(GCC_VERSION)" = "x4.4.1" ] ; then \
	PATH=$(BUILDPATH) \
	  $(MAKE) -C BUILD/build-pregcc-$(CROSSARCH) \
		  CFLAGS_FOR_TARGET="-O2 -g" \
		  CXXFLAGS_FOR_TARGET="-O2 -g" \
	    install-gcc install-target-libgcc ; \
	else \
	PATH=$(BUILDPATH) \
	  $(MAKE) -C BUILD/build-pregcc-$(CROSSARCH) \
		  CFLAGS_FOR_TARGET="-O2 -g" \
		  CXXFLAGS_FOR_TARGET="-O2 -g" \
	    install-gcc ; \
	fi
	cp $(SDKROOT)/lib/gcc/$(CROSSARCH)/$(GCC_VERSION)/libgcc.a \
		$(SDKROOT)/lib/gcc/$(CROSSARCH)/$(GCC_VERSION)/libgcc_eh.a
	touch $@

.PHONY: pregcc
pregcc: BUILD/stamp-pregcc

##################################################################
# newlib:
# Builds the bare-bones library used by NativeClient applications.
# NOTE: removes the default pthread.h to enable correct install
# by the Native Client threads package build.
##################################################################
BUILD/stamp-newlib: BUILD/stamp-newlib-$(NEWLIB_VERSION) Makefile
	rm -rf BUILD/build-newlib-$(CROSSARCH)
	mkdir BUILD/build-newlib-$(CROSSARCH)
	../src/trusted/service_runtime/export_header.py ../src/trusted/service_runtime/include \
		BUILD/newlib-$(NEWLIB_VERSION)/newlib/libc/sys/nacl
	if [ "x$(CROSSARCH)" = "xnacl64" ] ; then \
	  cd BUILD/build-newlib-$(CROSSARCH) && \
	    PATH=$(BUILDPATH) \
		  CFLAGS="-march=x86-64" \
	      ../newlib-$(NEWLIB_VERSION)/configure \
			  --enable-newlib-io-long-long \
			  --enable-newlib-io-c99-formats \
	        --prefix=$(SDKROOT) \
	        --target=$(CROSSARCH) && \
	  cd ../.. ; \
	  PATH=$(BUILDPATH) \
	    $(MAKE) -C BUILD/build-newlib-$(CROSSARCH) \
		    CFLAGS_FOR_TARGET="-march=x86-64 -O2" \
	          CXXFLAGS_FOR_TARGET="-march=x86-64 -O2" \
	      all ; \
	else \
	  cd BUILD/build-newlib-$(CROSSARCH) && \
	    PATH=$(BUILDPATH) \
		  CFLAGS="-m32 -march=i486 -msse2 -mfpmath=sse" \
	      ../newlib-$(NEWLIB_VERSION)/configure \
			  --enable-newlib-io-long-long \
			  --enable-newlib-io-c99-formats \
	        --prefix=$(SDKROOT) \
	        --target=$(CROSSARCH) ; \
	  cd ../.. ; \
	  PATH=$(BUILDPATH) \
	    $(MAKE) -C BUILD/build-newlib-$(CROSSARCH) \
		    CFLAGS_FOR_TARGET="-m32 -march=i486 -msse2 -mfpmath=sse -O2" \
	          CXXFLAGS_FOR_TARGET="-m32 -march=i486 -msse2 -mfpmath=sse -O2" \
	      all ; \
	fi
	PATH=$(BUILDPATH) \
	  $(MAKE) -C BUILD/build-newlib-$(CROSSARCH) \
	    install_root=$(SDKROOT)/$(CROSSARCH) install
	rm $(SDKROOT)/$(CROSSARCH)/include/pthread.h
	touch $@

.PHONY: newlib
newlib: BUILD/stamp-newlib

##################################################################
# gcc:
# Builds the gcc that will be used to build applications.
##################################################################
BUILD/stamp-gcc: BUILD/stamp-gcc-$(GCC_VERSION) Makefile
	rm -rf BUILD/build-gcc-$(CROSSARCH)
	mkdir BUILD/build-gcc-$(CROSSARCH)
	cd BUILD/build-gcc-$(CROSSARCH) && \
	  PATH=$(BUILDPATH) CC="gcc" \
		CFLAGS="-Dinhibit_libc -DNACL_ALIGN_BYTES=$(DEFAULT_ALIGN_BYTES) -DNACL_ALIGN_POW2=$(DEFAULT_ALIGN_POW2)" \
		CFLAGS_FOR_TARGET="-O2 -g" \
		CXXFLAGS_FOR_TARGET="-O2 -g" \
	  LDFLAGS="-s" \
	    ../gcc-$(GCC_VERSION)/configure \
	      --prefix=$(SDKROOT) \
				--with-newlib \
	      --enable-threads=nacl \
	      --enable-tls \
	      --disable-libmudflap \
	      --disable-decimal-float \
	      --disable-libssp \
	      --disable-shared \
				--disable-libgomp \
	      --disable-libstdcxx-pch \
	      --enable-languages="c,c++" \
	      --target=$(CROSSARCH)
	PATH=$(BUILDPATH) \
	  $(MAKE) -C BUILD/build-gcc-$(CROSSARCH) \
		  CFLAGS_FOR_TARGET="-O2 -g" \
		  CXXFLAGS_FOR_TARGET="-O2 -g" \
	    all
	PATH=$(BUILDPATH) \
	  $(MAKE) -C BUILD/build-gcc-$(CROSSARCH) \
		  CFLAGS_FOR_TARGET="-O2 -g" \
		  CXXFLAGS_FOR_TARGET="-O2 -g" \
	    install
	touch $@

.PHONY: gcc
gcc: BUILD/stamp-gcc

##################################################################
# gdb:
# Builds gdb.
##################################################################
# FIXME: This currently forces a 32-bit host.
# Only linux is supported.
BUILD/stamp-gdb: BUILD/stamp-gdb-$(GDB_VERSION) #Makefile
	case $$(BUILD/gdb-$(GDB_VERSION)/config.guess) in \
	*-linux*) ;; \
	*) echo "Unsupported host" >&2 ; false ;; \
	esac
	rm -rf BUILD/build-gdb-$(CROSSARCH)
	mkdir BUILD/build-gdb-$(CROSSARCH)
	CC=gcc ; \
	BUILD= ; \
	case $$(BUILD/gdb-$(GDB_VERSION)/config.guess) in \
	x86_64-*) CC="gcc -m32" ; BUILD=--build=i686-linux ;; \
	esac ; \
	cd BUILD/build-gdb-$(CROSSARCH) && \
	  CC="$${CC}" \
	  LDFLAGS="-s" \
	  ../gdb-$(GDB_VERSION)/configure \
	    --prefix=$(SDKROOT) \
	    $${BUILD} \
	    --target=${CROSSARCH}
	$(MAKE) -C BUILD/build-gdb-$(CROSSARCH) all
	$(MAKE) -C BUILD/build-gdb-$(CROSSARCH) install
	touch $@

.PHONY: gdb
gdb: BUILD/stamp-gdb

##################################################################
# Build the libraries and c runtime stubs.
##################################################################
nc_threads: ALWAYS
	cd .. && \
	 ./$(SCONS) naclsdk_mode=custom:$(SDKLOC_NATIVE)/nacl-sdk \
	 MODE=nacl_extra_sdk install_libpthread --verbose

# The copy of the header files first is a hack.
# TODO(sehr): fix the missing dependencies in the SDK library build.
nacl_libraries: ALWAYS
	cd .. && \
	 ./$(SCONS) naclsdk_mode=custom:$(SDKLOC_NATIVE)/nacl-sdk \
	 MODE=nacl_extra_sdk extra_sdk_update_header --verbose \
	 ./$(SCONS) naclsdk_mode=custom:$(SDKLOC_NATIVE)/nacl-sdk \
	 MODE=nacl_extra_sdk extra_sdk_update --verbose

sdk_minimal_test: ALWAYS
	cd .. && \
	 ./$(SCONS) naclsdk_mode=custom:$(SDKLOC_NATIVE)/nacl-sdk \
	 MODE=nacl sdk_minimal_test --verbose


##################################################################
# Build the entire sdk
##################################################################

build:
	$(MAKE) -f $(THISMAKEFILE) sdkdirs
	$(CYGWIN_DLL_COPY)
	cp -f ../../third_party/gcc/COPYING $(SDKROOT)
	$(MAKE) -f $(THISMAKEFILE) BUILD/stamp-binutils
	$(MAKE) -f $(THISMAKEFILE) BUILD/stamp-pregcc
	$(MAKE) -f $(THISMAKEFILE) BUILD/stamp-newlib
	$(MAKE) -f $(THISMAKEFILE) nc_threads
	$(MAKE) -f $(THISMAKEFILE) BUILD/stamp-gcc
	case $$(BUILD/binutils-$(BINUTILS_VERSION)/config.guess) in \
	*-linux*) $(MAKE) -f $(THISMAKEFILE) BUILD/stamp-gdb ;; \
	esac
	$(MAKE) -f $(THISMAKEFILE) nacl_libraries
	$(MAKE) -f $(THISMAKEFILE) sdk_minimal_test

##################################################################
# Build an updated set of patches from the current build versions.
##################################################################

PRISTINE:
	mkdir PRISTINE
	cd PRISTINE && tar xf $(SRCDIR)/binutils/binutils-$(BINUTILS_VERSION).tar.bz2
	cd PRISTINE && tar xf $(SRCDIR)/newlib/newlib-$(NEWLIB_VERSION).tar.gz
	cd PRISTINE && tar xf $(SRCDIR)/gcc/gcc-core-$(GCC_VERSION).tar.bz2
	cd PRISTINE && tar xf $(SRCDIR)/gcc/gcc-g++-$(GCC_VERSION).tar.bz2
	cd PRISTINE && tar xf $(SRCDIR)/gdb/gdb-$(GDB_VERSION).tar.bz2

BUILD/binutils_patchfile: | PRISTINE
	-(cd BUILD; \
    diff -Naur ../PRISTINE/binutils-$(BINUTILS_VERSION) \
				binutils-$(BINUTILS_VERSION) >  binutils_patchfile)

BUILD/newlib_patchfile: | PRISTINE
	-(cd BUILD; \
    diff -Naur ../PRISTINE/newlib-$(NEWLIB_VERSION) \
				newlib-$(NEWLIB_VERSION) >  newlib_patchfile)

BUILD/gcc_patchfile: | PRISTINE
	-(cd BUILD; \
    diff -Naur ../PRISTINE/gcc-$(GCC_VERSION) \
				gcc-$(GCC_VERSION) >  gcc_patchfile)

BUILD/gdb_patchfile: | PRISTINE
	-(cd BUILD; \
    diff -Naur ../PRISTINE/gdb-$(GDB_VERSION) \
				gdb-$(GDB_VERSION) >  gdb_patchfile)

patches: BUILD/binutils_patchfile \
				 BUILD/newlib_patchfile \
				 BUILD/gcc_patchfile \
				 BUILD/gdb_patchfile

##################################################################
# Build an SDK tarball.
##################################################################
TARBALL_FILE=$(shell pwd)/BUILD/nacl-sdk.tar
tarball:
	(cd $(SDKLOC); tar -cvhf $(TARBALL_FILE) $(SDKNAME); gzip $(TARBALL_FILE))

##################################################################
# Remove the BUILD directory and PRISTINE versions used for patches.
# Library builds are maintained by scons.
##################################################################
clean:
	-rm -rf BUILD PRISTINE

.PHONY: ALWAYS
