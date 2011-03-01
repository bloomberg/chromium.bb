# Copyright 2008, 2009, Google Inc.
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

##############################################################################
#  Description: Main makefile for NativeClient toolchain.
#
#  Usage:
#    Option 1: Build by unpacking the tarballs and applying patches:
#      goto "Perform the actual build"
#    Option 2: Build by copying the sources from the Git repository:
#      git clone http://git.chromium.org/git/nacl-toolchain.git SRC
#      make prepare-git-src
#    Perform the actual build:
#      make SDKLOC=/your/path clean build
#
#  Usage (building glibc, 32bit):
#    Option 1: Build by unpacking the tarballs and applying patches:
#      goto "Perform the actual build"
#    Option 2: Build by copying the sources from Git repositories:
#      git clone http://git.chromium.org/git/nacl-toolchain.git SRC
#      make fetch-glibc-sources-readonly
#      make prepare-git-src
#    Option 3: Committer access:
#      make fetch-git-rw-sources
#    Perform the actual build:
#      make build-with-glibc CROSSARCH=nacl MULTILIB=no SDKLOC=/your/path
#    Run Native Client tests:
#      make nacl-check CROSSARCH=nacl SDKLOC=/your/path
#
#  WARNING:
#    This Makefile does *not* support incremental rebuilds of individial
#  components (binutils, pregcc, gcc, newlib, gdb). Although you can force
#  rebuilding one of the components by deleting their respective stamps, this
#  does not always work. Use at your own risk.
##############################################################################

default: build

THISMAKEFILE := $(lastword $(MAKEFILE_LIST))

SHELL=/bin/bash

CROSSARCH=nacl64
GITRW=ssh://git@gitrw.chromium.org:9222
GITRO=http://git.chromium.org/git
GIT_BASE_URL=http://git.chromium.org/git
TOOLCHAINLOC?=out
SDKLOC?=$(abspath $(TOOLCHAINLOC))
SDKNAME=nacl-sdk
SDKROOT=$(SDKLOC)/$(SDKNAME)
DESTDIR?=
BUILDNCCDIR=$(SDKROOT)/$(CROSSARCH)
include REVISIONS

# Toplevel installation directory.
# MUST be an absolute pathname, for configure --prefix=$(PREFIX)
PREFIX=$(abspath $(SDKROOT))

# Convert CROSSARCH (nacl or nacl64) to (32 or 64).
BITSPLATFORM=$(subst 3264,64,$(subst nacl,32,$(CROSSARCH)))

LINUX_HEADERS="$(abspath $(dir $(THISMAKEFILE)))/SRC/linux-headers-for-nacl/include"
HPREFIX=BUILD/build-nacl-headers
NACL_SYS_HEADERS= \
      "$(HPREFIX)/sys/nacl_imc_api.h" \
      "$(HPREFIX)/sys/nacl_syscalls.h" \
      "$(HPREFIX)/sys/audio_video.h"
NACL_BITS_HEADERS= \
      "$(HPREFIX)/bits/nacl_imc_api.h" \
      "$(HPREFIX)/bits/nacl_syscalls.h"
NACL_MACHINE_HEADERS="$(HPREFIX)/machine/_types.h"

ifeq ($(shell uname -s), Darwin)
  PLATFORM=osx
  # Ensure that the resulting toolchain works on Mac OS X 10.5, since that
  # is not the default in newer versions of Xcode.
  export MACOSX_DEPLOYMENT_TARGET=10.5
else
  # No 'uname -o' on OSX
  PLATFORM:=$(shell uname -o)
endif

SRCDIR=../../../third_party

ifeq ($(PLATFORM), Cygwin)
  # Ugh, Cygwin and spaces in paths don't work well.
  # I'm explicitly coding the path.
  BUILDPATH=$(DESTDIR)$(SDKROOT)/bin:/usr/local/bin:/usr/bin:/bin
  SCONS?=scons.bat
  SDKLOC_NATIVE=$(shell cygpath -m $(SDKLOC))
  CYGWIN17=$(shell grep '^cygwin cygwin-1.7' /etc/setup/installed.db | wc -l)
  ifeq ($(CYGWIN17), 1)
    DESTDIR := $(SDKROOT)
    SDKROOT :=
    # CygWin1.7 changes over time and list of files required to run the binaries
    # changes over time - use cygcheck (new tools in CygWin 1.7) to find list of
    # .dll files and filter only .dll files in /bin (AKA /usr/bin depending on
    # exact version of CygWin)
    CYGWIN_DLL_COPY= \
      cp -f `cygpath \` \
        cd $(DESTDIR)$(SDKROOT)/libexec ; \
        for i in '*.exe' ; do \
          PATH=".;\\\`cygpath -w /usr/bin\\\`" /usr/bin/cygcheck $$i ; \
        done \` | \
      grep '\(/usr/\|\)bin/.*dll' | tr -d "\`printf '\r'\`" | sort -u` \
      $(DESTDIR)$(SDKROOT)/libexec ; \
      cp -f `cygpath \` \
        cd $(DESTDIR)$(SDKROOT)/libexec/gcc/$(CROSSARCH)/$(GCC_VERSION) ; \
        for i in '*.exe' ; do \
          PATH=".;\\\`cygpath -w /usr/bin\\\`" /usr/bin/cygcheck $$i ; \
        done \` | \
      grep '\(/usr/\|\)bin/.*dll' | tr -d "\`printf '\r'\`" | sort -u` \
      $(DESTDIR)$(SDKROOT)/libexec/gcc/$(CROSSARCH)/$(GCC_VERSION) || true
  else
    CYGWIN_DLL_COPY= \
      cp -f /cygdrive/c/cygwin/bin/cygwin1.dll \
      /cygdrive/c/cygwin/bin/cygiconv*.dll \
      /cygdrive/c/cygwin/bin/cygintl*.dll \
      /cygdrive/c/cygwin/bin/cygz*.dll \
      $(DESTDIR)$(SDKROOT)/libexec
 endif
else
  BUILDPATH=$(DESTDIR)$(SDKROOT)/bin:$(PATH)
  SCONS?=scons
  SDKLOC_NATIVE=$(DESTDIR)$(SDKLOC)
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
GCC_VERSION=4.4.3
GMP_VERSION=4.3.1
MPFR_VERSION=2.4.1

##################################################################
# Unpacking the sources, applying patches, etc.
##################################################################

# Create the SRC subdirectory.
SRC:
	mkdir SRC

unpack-src: stamp-src

# Prepare all sources.
stamp-src: SRC/stamp-binutils SRC/stamp-gcc SRC/stamp-newlib SRC/stamp-gdb | SRC
	touch stamp-src

# Prepare source directory from Git for building.
prepare-git-src: ALWAYS
	./use_gcc_extras.sh GMP_VERSION=$(GMP_VERSION) MPFR_VERSION=$(MPFR_VERSION)
	touch SRC/stamp-binutils
	touch SRC/stamp-gcc
	touch SRC/stamp-newlib
	touch SRC/stamp-gdb
	touch stamp-src

# Prepare glibc sources (checked out by hand) for building.
prepare-git-glibc-src: ALWAYS
	touch SRC/stamp-glibc
	touch SRC/stamp-linux-headers

fetch-git-rw-sources: ALWAYS
	if [ -d SRC ] ; then \
	  echo "Error: SRC exists." \
	    "Remove the source directory before fetching new sources." ; \
	else \
	  git clone $(GITRW)/nacl-toolchain.git SRC && \
	  $(MAKE) -f $(THISMAKEFILE) prepare-git-src && \
	  git clone $(GITRW)/nacl-glibc.git SRC/glibc && \
	  touch SRC/stamp-glibc && \
	  $(MAKE) -f $(THISMAKEFILE) fetch-glibc-sources-readonly ; \
	fi

# Unpack binutils, apply patches.
SRC/binutils: | SRC
	@echo "Unpacking binutils"
	git clone $(GIT_BASE_URL)/nacl-binutils SRC/binutils
	cd SRC/binutils && git checkout "$(NACL_BINUTILS_COMMIT)"

# Create binutils source directory, and mark that we've done that.
SRC/stamp-binutils: | SRC
	rm -rf SRC/binutils
	$(MAKE) -f $(THISMAKEFILE) SRC/binutils
	touch $@

# Unpack newlib, apply patches.
SRC/newlib: | SRC
	@echo "Unpacking newlib"
	git clone $(GIT_BASE_URL)/nacl-newlib SRC/newlib
	cd SRC/newlib && git checkout "$(NACL_NEWLIB_COMMIT)"

# Create newlib source directory, and mark that we've done that.
SRC/stamp-newlib: | SRC
	rm -rf SRC/newlib
	$(MAKE) -f $(THISMAKEFILE) SRC/newlib
	touch $@

# Unpack gcc, apply patches.
SRC/gcc: | SRC
	@echo "Unpacking gcc"
	git clone $(GIT_BASE_URL)/nacl-gcc SRC/gcc
	cd SRC/gcc && git checkout "$(NACL_GCC_COMMIT)"
	./use_gcc_extras.sh GMP_VERSION=$(GMP_VERSION) MPFR_VERSION=$(MPFR_VERSION)

# Create gcc source directory, and mark that we've done that.
SRC/stamp-gcc: | SRC
	rm -rf SRC/gcc
	$(MAKE) -f $(THISMAKEFILE) SRC/gcc
	touch $@

fetch-glibc-sources-readonly: SRC/stamp-glibc SRC/stamp-linux-headers
	@echo "Fetching glibc and linux headers done."

# Create glibc source directory, and mark that we've done that.
SRC/stamp-glibc: | SRC
	rm -rf SRC/glibc
	@echo "Fetching glibc"
	git clone $(GITRO)/nacl-glibc.git SRC/glibc
	touch $@

# Create linux headers source directory, and mark that we've done that.
SRC/stamp-linux-headers: | SRC
	@echo "Fetching linux headers for glibc."
	rm -rf SRC/linux-headers-for-nacl
	git clone $(GITRO)/linux-headers-for-nacl.git \
	  SRC/linux-headers-for-nacl
	cd SRC/linux-headers-for-nacl && \
	  git checkout 0014b8d2e052dd3bf4cdc4be999d17e123c50dc2
	touch $@

# Unpack gdb, apply patches.
SRC/gdb: | SRC
	@echo "Unpacking gdb"
	git clone $(GIT_BASE_URL)/nacl-gdb SRC/gdb
	cd SRC/gdb && git checkout "$(NACL_GDB_COMMIT)"

# Create gdb source directory, and mark that we've done that.
SRC/stamp-gdb: | SRC
	rm -rf SRC/gdb
	$(MAKE) -f $(THISMAKEFILE) SRC/gdb
	touch $@

# Create the build directories for compiled binaries.
BUILD:
	mkdir BUILD

##################################################################
# Create the SDK output directories.
##################################################################
SDKDIRS=\
    $(DESTDIR)$(SDKROOT)/bin \
    $(DESTDIR)$(SDKROOT)/lib \
    $(DESTDIR)$(SDKROOT)/libexec \
    $(DESTDIR)$(SDKROOT)/$(CROSSARCH)/include \
    $(DESTDIR)$(SDKROOT)/$(CROSSARCH)/lib \

.PHONY: sdkdirs
sdkdirs:
	echo "Creating the SDK tree at $(DESTDIR)$(SDKROOT)"
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
BUILD/stamp-$(CROSSARCH)-binutils: SRC/stamp-binutils | BUILD
	rm -rf BUILD/build-binutils-$(CROSSARCH)
	mkdir BUILD/build-binutils-$(CROSSARCH)
	# We'd like to build binutils with -Werror, but there are a
	# number of warnings in the Mac version of GCC that prevent
	# us from building with -Werror today.
	cd BUILD/build-binutils-$(CROSSARCH) && \
	  CC="gcc" \
	  CFLAGS="-m32 -O2 -DNACL_ALIGN_BYTES=$(DEFAULT_ALIGN_BYTES) -DNACL_ALIGN_POW2=$(DEFAULT_ALIGN_POW2)" \
	  LDFLAGS="-s" \
	  ../../SRC/binutils/configure \
	    --prefix=$(SDKROOT) \
	    --target=${CROSSARCH} \
	    --disable-werror
	$(MAKE) -C BUILD/build-binutils-$(CROSSARCH) all
	$(MAKE) -C BUILD/build-binutils-$(CROSSARCH) DESTDIR=$(DESTDIR) install
	touch $@
	$(MAKE) -f $(THISMAKEFILE) redirector

.PHONY: binutils
binutils: BUILD/stamp-$(CROSSARCH)-binutils

##################################################################
# pregcc:
# Builds the cross gcc used to build the libraries.
##################################################################

BUILD/stamp-$(CROSSARCH)-pregcc: SRC/stamp-gcc | BUILD
	rm -rf BUILD/build-pregcc-$(CROSSARCH)
	mkdir BUILD/build-pregcc-$(CROSSARCH)
	PATH=$(BUILDPATH) $(MAKE) \
	    -C BUILD/build-pregcc-$(CROSSARCH) \
	    -f ../../Makefile.gcc \
	    prefix=$(SDKROOT) \
	    srcdir=../../SRC/gcc \
	    target=$(CROSSARCH) \
	    DEFAULT_ALIGN_BYTES=$(DEFAULT_ALIGN_BYTES) \
	    DEFAULT_ALIGN_POW2=$(DEFAULT_ALIGN_POW2) \
	    MULTILIB=$(MULTILIB) \
	    configure-pregcc
	PATH=$(BUILDPATH) $(MAKE) \
	    -C BUILD/build-pregcc-$(CROSSARCH) \
	    all-gcc all-target-libgcc
	PATH=$(BUILDPATH) $(MAKE) \
	    -C BUILD/build-pregcc-$(CROSSARCH) \
	    DESTDIR=$(DESTDIR) \
	    install-gcc install-target-libgcc
	cp $(DESTDIR)$(SDKROOT)/lib/gcc/$(CROSSARCH)/$(GCC_VERSION)/libgcc.a \
		$(DESTDIR)$(SDKROOT)/lib/gcc/$(CROSSARCH)/$(GCC_VERSION)/libgcc_eh.a
	cp $(DESTDIR)$(SDKROOT)/lib/gcc/$(CROSSARCH)/$(GCC_VERSION)/32/libgcc.a \
		$(DESTDIR)$(SDKROOT)/lib/gcc/$(CROSSARCH)/$(GCC_VERSION)/32/libgcc_eh.a |\
	true
	touch $@
	$(MAKE) -f $(THISMAKEFILE) redirector

.PHONY: pregcc
pregcc: BUILD/stamp-$(CROSSARCH)-pregcc


##################################################################
# pregcc-standalone:
# Builds the cross gcc used to build glibc.
# TODO(eaeltsin): now works for Linux only, enable for Windows/Mac
# TODO(eaeltsin): get rid of pregcc in favor of pregcc-standalone
##################################################################

# Toplevel installation directory for pregcc.
# MUST be an absolute pathname, for configure --prefix=$(PREGCC_PREFIX)
# Pregcc is installed separately so that it is not overwritten with full gcc.
# Pregcc is needed for rebuilding glibc, while full gcc can't do that because
# of its incompatible libgcc.
PREGCC_PREFIX=$(abspath BUILD/install-pregcc-$(CROSSARCH))

# Build directory for pregcc.
PREGCC_BUILD_DIR=BUILD/build-pregcc-$(CROSSARCH)

# Build pregcc:
# create links to binutils:
#   Alternate approaches are to make PATH point to nacl binutils or to use
#   pregcc with -B option. Both seem unreliable, as after full gcc is installed
#   the search path will include full gcc stuff that should not be picked.
# make install:
#   DESTDIR should be ignored at this step.
BUILD/stamp-$(CROSSARCH)-pregcc-standalone: SRC/stamp-gcc | BUILD
	rm -rf $(PREGCC_PREFIX)
	mkdir -p $(PREGCC_PREFIX)/$(CROSSARCH)/bin
	for f in '$(PREFIX)/$(CROSSARCH)/bin/*'; do \
	    ln -s $$f $(PREGCC_PREFIX)/$(CROSSARCH)/bin; \
	    done
	rm -rf $(PREGCC_BUILD_DIR)
	mkdir $(PREGCC_BUILD_DIR)
	$(MAKE) \
	    -C $(PREGCC_BUILD_DIR) \
	    -f ../../Makefile.gcc \
	    prefix=$(PREGCC_PREFIX) \
	    srcdir=../../SRC/gcc \
	    target=$(CROSSARCH) \
	    DEFAULT_ALIGN_BYTES=$(DEFAULT_ALIGN_BYTES) \
	    DEFAULT_ALIGN_POW2=$(DEFAULT_ALIGN_POW2) \
	    MULTILIB=$(MULTILIB) \
	    configure-pregcc
	$(MAKE) \
	    -C $(PREGCC_BUILD_DIR) \
	    all-gcc \
	    all-target-libgcc
	$(MAKE) \
	    -C $(PREGCC_BUILD_DIR) \
	    install-gcc \
	    install-target-libgcc
	echo '/* Intentionally empty */' > \
	    $(PREGCC_PREFIX)/$(CROSSARCH)/lib/libnacl.so
	echo '/* Intentionally empty */' > \
	    $(PREGCC_PREFIX)/$(CROSSARCH)/lib/libcrt_platform.a
	touch $@


##################################################################
# newlib:
# Builds the bare-bones library used by NativeClient applications.
# NOTE: removes the default pthread.h to enable correct install
# by the Native Client threads package build.
##################################################################
BUILD/stamp-$(CROSSARCH)-newlib: SRC/stamp-newlib | BUILD
	rm -rf BUILD/build-newlib-$(CROSSARCH)
	mkdir BUILD/build-newlib-$(CROSSARCH)
	(cd $(DESTDIR)$(SDKROOT)/$(CROSSARCH) ; \
	    ln -sfn lib/32 lib32 ; \
	    ln -sfn lib lib64 ; \
	    cd $(DESTDIR)$(SDKROOT); \
	    rm -rf nacl; mkdir nacl; cd nacl; \
	    mkdir bin; \
	    ln -s ../nacl64/lib/32 lib; \
	    ln -s ../nacl64/include; )
	../src/trusted/service_runtime/export_header.py ../src/trusted/service_runtime/include \
		SRC/newlib/newlib/libc/sys/nacl
	if [ "x$(CROSSARCH)" = "xnacl64" ] ; then \
	  cd BUILD/build-newlib-$(CROSSARCH) && \
	    PATH=$(BUILDPATH) \
		  CFLAGS="-O2" \
	      ../../SRC/newlib/configure \
			  --disable-libgloss \
			  --enable-newlib-iconv \
			  --enable-newlib-io-long-long \
			  --enable-newlib-io-long-double \
			  --enable-newlib-io-c99-formats \
			  --enable-newlib-mb \
	        --prefix=$(SDKROOT) \
	        --target=$(CROSSARCH) && \
	  cd ../.. ; \
	  PATH=$(BUILDPATH) \
	    $(MAKE) -C BUILD/build-newlib-$(CROSSARCH) \
	                    CCASFLAGS="-D_I386MACH_ALLOW_HW_INTERRUPTS" \
		    CFLAGS_FOR_TARGET="-m64 -O2" \
	          CXXFLAGS_FOR_TARGET="-m64 -O2" \
	      all ; \
	else \
	  cd BUILD/build-newlib-$(CROSSARCH) && \
	    PATH=$(BUILDPATH) \
		  CFLAGS="-O2" \
	      ../../SRC/newlib/configure \
			  --disable-libgloss \
			  --enable-newlib-iconv \
			  --enable-newlib-io-long-long \
			  --enable-newlib-io-long-double \
			  --enable-newlib-io-c99-formats \
			  --enable-newlib-mb \
	        --prefix=$(SDKROOT) \
	        --target=$(CROSSARCH) ; \
	  cd ../.. ; \
	  PATH=$(BUILDPATH) \
	    $(MAKE) -C BUILD/build-newlib-$(CROSSARCH) \
		    CFLAGS_FOR_TARGET="-m32 -O2" \
	          CXXFLAGS_FOR_TARGET="-m32 -O2" \
	      all ; \
	fi
	PATH=$(BUILDPATH) \
	  $(MAKE) -C BUILD/build-newlib-$(CROSSARCH) \
	    DESTDIR=$(DESTDIR) install
	rm $(DESTDIR)$(SDKROOT)/$(CROSSARCH)/include/pthread.h
	touch $@

.PHONY: newlib
newlib: BUILD/stamp-$(CROSSARCH)-newlib


##################################################################
# glibc:
##################################################################

# Build directory for glibc.
GLIBC_BUILD_DIR=BUILD/build-glibc-$(CROSSARCH)

# Glibc is built with pregcc.
GLIBC_CC=$(PREGCC_PREFIX)/bin/$(CROSSARCH)-gcc

# Convert (nacl or nacl64) to (i486 or x86_64).
GLIBC_HOST=$(if $(filter 32,$(BITSPLATFORM)),i486,x86_64)-linux-gnu

# Convert (nacl or nacl64) to ("-march=i486" or "").
GLIBC_HOST_CFLAGS=$(if $(filter 32,$(BITSPLATFORM)),-march=i486,)

# CFLAGS for building glibc.
GLIBC_CFLAGS=-O2 -g

# Extra arguments to scons to define build and install destinations.
SCONS_DESTINATIONS=\
 DESTINATION_ROOT=tools/BUILD/build-extra_sdk-$(CROSSARCH) \
 naclsdk_mode=custom:$(SDKLOC_NATIVE)/$(SDKNAME) \
 extra_sdk_lib_destination=$(DESTDIR)$(SDKROOT)/$(CROSSARCH)/lib \
 extra_sdk_include_destination=$(DESTDIR)$(SDKROOT)/$(CROSSARCH)/include \

# Build glibc:
BUILD/stamp-$(CROSSARCH)-glibc: SRC/stamp-glibc SRC/stamp-linux-headers
	rm -rf $(GLIBC_BUILD_DIR)
	mkdir -p $(GLIBC_BUILD_DIR)/lib
	cd $(GLIBC_BUILD_DIR) && ../../SRC/glibc/configure \
	    BUILD_CC="gcc -O2 -g" \
	    CC=$(GLIBC_CC) \
	    CFLAGS="$(GLIBC_CFLAGS) $(GLIBC_HOST_CFLAGS) -pipe -fno-strict-aliasing -mno-tls-direct-seg-refs" \
	    libc_cv_forced_unwind=yes \
	    libc_cv_c_cleanup=yes \
	    --prefix=$(PREFIX)/$(CROSSARCH) \
	    --host=$(GLIBC_HOST) \
	    --with-headers=$(LINUX_HEADERS) \
	    --enable-kernel=2.2.0
	$(MAKE) -C $(GLIBC_BUILD_DIR)
	$(MAKE) -C $(GLIBC_BUILD_DIR) DESTDIR=$(DESTDIR) install
	touch $@


##################################################################
# Ad hoc linker scripts and a selection of NaCl headers for GCC.
##################################################################
BUILD/stamp-$(CROSSARCH)-glibc-adhoc-files:
	cp -f SRC/glibc/nacl/dyn-link/ldscripts/* \
	    $(DESTDIR)$(SDKROOT)/$(CROSSARCH)/lib/ldscripts/
	cp -rf $(LINUX_HEADERS)/* $(DESTDIR)$(SDKROOT)/$(CROSSARCH)/include
	rm -rf $(HPREFIX)
	../src/trusted/service_runtime/export_header.py \
	  ../src/trusted/service_runtime/include $(HPREFIX)
	mkdir -p $(DESTDIR)$(SDKROOT)/$(CROSSARCH)/include/{sys,machine}
	cp -f $(NACL_BITS_HEADERS) $(DESTDIR)$(SDKROOT)/$(CROSSARCH)/include/bits
	cp -f $(NACL_SYS_HEADERS) $(DESTDIR)$(SDKROOT)/$(CROSSARCH)/include/sys
	cp -f $(NACL_MACHINE_HEADERS) $(DESTDIR)$(SDKROOT)/$(CROSSARCH)/include/machine
	cp ../src/untrusted/include/machine/_default_types.h \
		$(DESTDIR)$(SDKROOT)/$(CROSSARCH)/include/machine/_default_types.h
	echo '/* Intentionally empty */' > \
		$(DESTDIR)$(SDKROOT)/$(CROSSARCH)/lib/libnacl.so
	echo '/* Intentionally empty */' > \
		$(DESTDIR)$(SDKROOT)/$(CROSSARCH)/lib/libcrt_platform.a
	echo '/* Intentionally empty */' > \
		$(DESTDIR)$(SDKROOT)/$(CROSSARCH)/lib/libnosys.so
	cd $(DESTDIR)$(SDKROOT)/$(CROSSARCH) && \
		rm -rf lib$(BITSPLATFORM) && ln -s lib lib$(BITSPLATFORM)
	touch $@

##################################################################
# Native Client libraries.
##################################################################
BUILD/stamp-$(CROSSARCH)-libs-glibc: BUILD/stamp-$(CROSSARCH)-glibc-adhoc-files
	rm -rf BUILD/build-extra_sdk-$(CROSSARCH)
	mkdir BUILD/build-extra_sdk-$(CROSSARCH)
	cd .. && \
	  (./$(SCONS) \
	    $(SCONS_DESTINATIONS) \
	    MODE=nacl_extra_sdk --nacl_glibc \
	    extra_sdk_update_header extra_sdk_update \
	    targetplatform=x86-$(BITSPLATFORM) --verbose )
	# Remove the fake libnacl.so to let libnacl.a be used to run tests.
	if [ -f $(DESTDIR)$(SDKROOT)/$(CROSSARCH)/lib/libnacl.a ]; then \
	  rm -f $(DESTDIR)$(SDKROOT)/$(CROSSARCH)/lib/libnacl.so; \
	fi
	touch $@

NCVAL_STUBOUT=$(abspath BUILD/build-extra_sdk-$(CROSSARCH)/opt-linux-x86-$(BITSPLATFORM)/staging/ncval_stubout)
BUILD/stamp-$(CROSSARCH)-stubout:
	cd .. && ./$(SCONS) --mode=opt-linux platform=x86-$(BITSPLATFORM) \
	    $(SCONS_DESTINATIONS) ncval_stubout
	set -e; find $(DESTDIR)$(SDKROOT)/$(CROSSARCH)/lib -name '*.so' | \
	  while read library ; do \
	    if [ "$$(head -c 4 "$$library" | tail -c 3)" = 'ELF' ] ; then \
	      $(NCVAL_STUBOUT) "$$library" -o "$$library"; \
	    fi \
	  done
	touch $@

.PHONY: stubout
stubout: BUILD/stamp-$(CROSSARCH)-stubout


##################################################################
# gcc:
#   Builds GCC with glibc as a C library.
##################################################################
BUILD/stamp-$(CROSSARCH)-full-gcc: BUILD/stamp-$(CROSSARCH)-glibc-adhoc-files
	rm -rf BUILD/build-full-gcc-$(CROSSARCH)
	mkdir BUILD/build-full-gcc-$(CROSSARCH)
	# See http://code.google.com/p/nativeclient/issues/detail?id=854
	rm -f $(DESTDIR)$(SDKROOT)/$(CROSSARCH)/sys-include
	mkdir $(DESTDIR)$(SDKROOT)/$(CROSSARCH)/sys-include
	ln -s ../include/limits.h $(DESTDIR)$(SDKROOT)/$(CROSSARCH)/sys-include/limits.h
	PATH=$(BUILDPATH) $(MAKE) \
	    -C BUILD/build-full-gcc-$(CROSSARCH) \
	    -f ../../Makefile.gcc \
	    prefix=$(SDKROOT) \
	    srcdir=../../SRC/gcc \
	    target=$(CROSSARCH) \
	    DEFAULT_ALIGN_BYTES=$(DEFAULT_ALIGN_BYTES) \
	    DEFAULT_ALIGN_POW2=$(DEFAULT_ALIGN_POW2) \
	    MULTILIB=$(MULTILIB) \
	    configure-gcc-with-glibc
	PATH=$(BUILDPATH) $(MAKE) \
	    -C BUILD/build-full-gcc-$(CROSSARCH) \
	    all
	PATH=$(BUILDPATH) $(MAKE) \
	    -C BUILD/build-full-gcc-$(CROSSARCH) \
	    DESTDIR=$(DESTDIR) \
	    install
	touch $@

##################################################################
# gcc:
# Builds the gcc that will be used to build applications.
##################################################################
BUILD/stamp-$(CROSSARCH)-gcc: SRC/stamp-gcc | BUILD
	rm -rf BUILD/build-gcc-$(CROSSARCH)
	mkdir BUILD/build-gcc-$(CROSSARCH)
ifeq ($(CYGWIN17), 1)
	mkdir BUILD/build-gcc-$(CROSSARCH)/gcc
	cp -af $(DESTDIR)$(SDKROOT)/libexec/cyg*.dll \
	    BUILD/build-gcc-$(CROSSARCH)/gcc
	cp -af $(DESTDIR)$(SDKROOT)/$(CROSSARCH) \
	    BUILD/build-gcc-$(CROSSARCH)/$(CROSSARCH)
endif
	PATH=$(BUILDPATH) $(MAKE) \
	    -C BUILD/build-gcc-$(CROSSARCH) \
	    -f ../../Makefile.gcc \
	    prefix=$(SDKROOT) \
	    srcdir=../../SRC/gcc \
	    target=$(CROSSARCH) \
	    DEFAULT_ALIGN_BYTES=$(DEFAULT_ALIGN_BYTES) \
	    DEFAULT_ALIGN_POW2=$(DEFAULT_ALIGN_POW2) \
	    MULTILIB=$(MULTILIB) \
	    configure-gcc-with-newlib
	PATH=$(BUILDPATH) $(MAKE) \
	    -C BUILD/build-gcc-$(CROSSARCH) \
	    all
	PATH=$(BUILDPATH) $(MAKE) \
	    -C BUILD/build-gcc-$(CROSSARCH) \
	    DESTDIR=$(DESTDIR) \
	    install
	$(MAKE) -f $(THISMAKEFILE) redirector
ifeq ($(PLATFORM), Cygwin)
	for exe in "$(DESTDIR)$(SDKROOT)/$(CROSSARCH)/bin/"* ; do \
	  echo "The next command can hang on FAT partiton; use NTFS!" ; \
	  ln -fn redirector.exe \
	    "$(DESTDIR)$(SDKROOT)/nacl/bin/`basename $$exe`"; \
	done; \
	for exe in "$(DESTDIR)$(SDKROOT)/bin/$(CROSSARCH)-"*.exe ; do \
	  N="`basename $$exe`"; \
	  echo "The next command can hang on FAT partiton; use NTFS!" ; \
	  ln -fn redirector.exe \
	    "$(DESTDIR)$(SDKROOT)/bin/nacl-$${N/$(CROSSARCH)-}"; \
	  ln -fn "$(DESTDIR)$(SDKROOT)/libexec/$$N" \
	    "$(DESTDIR)$(SDKROOT)/libexec/nacl-$${N/$(CROSSARCH)-}"; \
	done; \
	for exe in "$(DESTDIR)$(SDKROOT)/bin/$(CROSSARCH)-"* ; do \
	  N="`basename $$exe`"; \
	  echo "The next command can hang on FAT partiton; use NTFS!" ; \
	  ln -n "$$exe" "$(DESTDIR)$(SDKROOT)/bin/nacl-$${N/$(CROSSARCH)-}"| \
	    true; \
	done;
else
	for exe in "$(DESTDIR)$(SDKROOT)/$(CROSSARCH)/bin/"* ; do \
	  ./create_redirector.sh \
	    -s "$(DESTDIR)$(SDKROOT)/bin/nacl-`basename $$exe`"; \
	  ./create_redirector.sh -t \
	    -s "$(DESTDIR)$(SDKROOT)/nacl/bin/`basename $$exe`"; \
	done; \
	for exe in "$(DESTDIR)$(SDKROOT)/bin/$(CROSSARCH)-"* ; do \
	  N="`basename $$exe`"; \
	  ln -n "$$exe" "$(DESTDIR)$(SDKROOT)/bin/nacl-$${N/$(CROSSARCH)-}"| \
	    true; \
	done;
endif
	touch $@

.PHONY: gcc
gcc: BUILD/stamp-$(CROSSARCH)-gcc

##################################################################
# gdb:
# Builds gdb.
##################################################################
# TODO: Add 64-bit support.
# Only linux is supported.
BUILD/stamp-$(CROSSARCH)-gdb: SRC/stamp-gdb | BUILD
	case $$(SRC/gdb/config.guess) in \
	*-linux*) ;; \
	*) echo "Unsupported host" >&2 ; false ;; \
	esac
	rm -rf BUILD/build-gdb-$(CROSSARCH)
	mkdir BUILD/build-gdb-$(CROSSARCH)
	CC=gcc ; \
	BUILD= ; \
	case $$(SRC/gdb/config.guess) in \
	x86_64-*) CC="gcc -m32" ; BUILD=--build=i686-linux ;; \
	esac ; \
	cd BUILD/build-gdb-$(CROSSARCH) && \
	  CC="$${CC}" \
	  LDFLAGS="-s" \
	  ../../SRC/gdb/configure \
	    --prefix=$(SDKROOT) \
	    $${BUILD} \
	    --target=nacl
	$(MAKE) -C BUILD/build-gdb-$(CROSSARCH) all
	$(MAKE) -C BUILD/build-gdb-$(CROSSARCH) DESTDIR=$(DESTDIR) install
	touch $@
	$(MAKE) -f $(THISMAKEFILE) redirector

.PHONY: gdb
gdb: BUILD/stamp-$(CROSSARCH)-gdb

##################################################################
# Build the libraries and c runtime stubs.
##################################################################
nc_threads: ALWAYS
	cd .. && \
	 ./$(SCONS) naclsdk_mode=custom:$(SDKLOC_NATIVE)/$(SDKNAME) \
	  --mode=nacl_extra_sdk install_libpthread --verbose \
	  targetplatform=x86-$(BITSPLATFORM)

nacl_libraries_nocpp: ALWAYS
	cd .. && \
	  (./$(SCONS) naclsdk_mode=custom:$(SDKLOC_NATIVE)/$(SDKNAME) \
	    --mode=nacl_extra_sdk extra_sdk_update_header \
	    nocpp=yes --verbose ) && \
	  (./$(SCONS) naclsdk_mode=custom:$(SDKLOC_NATIVE)/$(SDKNAME) \
	    --mode=nacl_extra_sdk extra_sdk_update \
	    nocpp=yes targetplatform=x86-32 --verbose ) && \
	  (./$(SCONS) naclsdk_mode=custom:$(SDKLOC_NATIVE)/$(SDKNAME) \
	    --mode=nacl_extra_sdk extra_sdk_update \
	    nocpp=yes targetplatform=x86-64 --verbose ) ;

##################################################################
# nacl_libraries:
# Injects nacl-specific libraries in toolchain.
##################################################################
# TODO(khim): make npapi headers in the Chrome repository to be compilable under
#             __native_client__ or remove them.
# TODO(khim): factor out {n,p}papi headers and libraries out of the toolchain.
#             Add them to SDK during the SDK build.
nacl_libraries: ALWAYS
	cd .. && \
	  (./$(SCONS) naclsdk_mode=custom:$(SDKLOC_NATIVE)/$(SDKNAME) \
	    --mode=nacl_extra_sdk extra_sdk_update_header --verbose ) && \
	  (./$(SCONS) naclsdk_mode=custom:$(SDKLOC_NATIVE)/$(SDKNAME) \
	    --mode=nacl_extra_sdk extra_sdk_update \
	    targetplatform=x86-32 --verbose ) && \
	  (./$(SCONS) naclsdk_mode=custom:$(SDKLOC_NATIVE)/$(SDKNAME) \
	    --mode=nacl_extra_sdk extra_sdk_update \
	    targetplatform=x86-64 --verbose ) ;
	chmod a-x "$(DESTDIR)$(SDKROOT)/"{nacl,"$(CROSSARCH)"}/lib/*.a
	rm -f "$(DESTDIR)$(SDKROOT)/"{nacl,"$(CROSSARCH)"}/lib/*.la

sdk_minimal_test: ALWAYS
	cd .. && \
	   ./$(SCONS) naclsdk_mode=custom:$(SDKLOC_NATIVE)/$(SDKNAME) \
	     --mode=nacl sdk_minimal_test platform=x86-32 --verbose;

gdb_minimal_test: BUILD/stamp-$(CROSSARCH)-gdb
	cd .. && \
	  ./$(SCONS) naclsdk_mode=custom:$(SDKLOC_NATIVE)/$(SDKNAME) \
	    --mode=nacl,dbg-linux platform=x86-32 gdb_minimal_test


##########################################################################
# Install CygWin 1.7 redirector
##########################################################################
# Two different cygwin1.dll files in %PATH% lead to trouble, but we'd like
# to have our as/gcc/ld in %PATH% in SDK thus simple trick is employed:
# we move compiled binary to libexec subdirectory and put very simple
# MSVC-compiled redirector in it's place. This only works with CygWin 1.7+
#
# Use redirector on other platforms to save space.

redirector: ALWAYS
ifeq ($(PLATFORM), Cygwin)
	for exe in "$(DESTDIR)$(SDKROOT)/bin/$(CROSSARCH)"-*.exe ; do \
	  if ! cmp redirector.exe "$$exe" > /dev/null ; then \
	    mv -f "$$exe" "$(DESTDIR)$(SDKROOT)/libexec/`basename $$exe`" ; \
	    echo "The next command can hang on FAT partiton; use NTFS!" ; \
	    ln -fn redirector.exe "$$exe" ; \
	  fi ; \
	done
	for exe in "$(DESTDIR)$(SDKROOT)/$(CROSSARCH)/bin/"*.exe ; do \
	  echo "The next command can hang on FAT partiton; use NTFS!" ; \
	  ln -fn redirector.exe $$exe ; \
	done
	$(CYGWIN_DLL_COPY)
	LEDIR="$(DESTDIR)$(SDKROOT)/libexec" ; \
	ARCHDIR="$$LEDIR/gcc/$(CROSSARCH)/$(GCC_VERSION)" ; \
	if [ -d "$$ARCHDIR" ] ; then \
	  for exe in "$$ARCHDIR/"*.dll ; do \
	    if [ -e "$$LEDIR/`basename $$exe`" ] ; then \
	      ln -fn "$$LEDIR/`basename $$exe`" "$$exe" ; \
	    fi ; \
	  done ; \
	fi
else
	for exe in "$(DESTDIR)$(SDKROOT)/$(CROSSARCH)/bin/"* ; do \
	  ln -sfn "../../bin/$(CROSSARCH)-`basename $$exe`" "$$exe" ; \
	done
endif

##################################################################
# Build the entire toolchain.
##################################################################

build-with-glibc: ALWAYS
	$(MAKE) -f $(THISMAKEFILE) sdkdirs
	cp -f SRC/gcc/COPYING* $(DESTDIR)$(SDKROOT)
	$(MAKE) -f $(THISMAKEFILE) BUILD/stamp-$(CROSSARCH)-binutils
	$(MAKE) -f $(THISMAKEFILE) BUILD/stamp-$(CROSSARCH)-pregcc-standalone
	$(MAKE) -f $(THISMAKEFILE) BUILD/stamp-$(CROSSARCH)-glibc
	$(MAKE) -f $(THISMAKEFILE) BUILD/stamp-$(CROSSARCH)-full-gcc
	$(MAKE) -f $(THISMAKEFILE) BUILD/stamp-$(CROSSARCH)-libs-glibc
	$(MAKE) -f $(THISMAKEFILE) BUILD/stamp-$(CROSSARCH)-stubout

all-source: | \
	SRC/stamp-binutils \
	SRC/stamp-gcc \
	SRC/stamp-newlib \
	SRC/stamp-gdb

build: ALWAYS all-source
	$(MAKE) -f $(THISMAKEFILE) sdkdirs
	cp -f SRC/gcc/COPYING* $(DESTDIR)$(SDKROOT)
	$(MAKE) -f $(THISMAKEFILE) BUILD/stamp-$(CROSSARCH)-binutils
	$(MAKE) -f $(THISMAKEFILE) BUILD/stamp-$(CROSSARCH)-pregcc
	$(MAKE) -f $(THISMAKEFILE) BUILD/stamp-$(CROSSARCH)-newlib
	$(MAKE) -f $(THISMAKEFILE) nc_threads
	# We need libnacl.a for gfortran, but we don't have a c++ yet
	$(MAKE) -f $(THISMAKEFILE) nacl_libraries_nocpp
	$(MAKE) -f $(THISMAKEFILE) BUILD/stamp-$(CROSSARCH)-gcc
	case $$(SRC/binutils/config.guess) in \
	*-linux*) $(MAKE) -f $(THISMAKEFILE) BUILD/stamp-$(CROSSARCH)-gdb ;; \
	esac
	$(MAKE) -f $(THISMAKEFILE) nacl_libraries
	$(MAKE) -f $(THISMAKEFILE) sdk_minimal_test
	$(MAKE) -f $(THISMAKEFILE) redirector

##################################################################
# Run DejaGnu tests.
##################################################################

DEJAGNU_TIMESTAMP := $(shell date +%y%m%d%H%M%S)

.PHONY: check
check: ALWAYS
	(cd .. && \
	  ./$(SCONS) naclsdk_mode=custom:$(DESTDIR)$(SDKROOT) \
	    --mode=dbg-linux,nacl platform=x86-$(BITSPLATFORM) \
	    sdl=none --verbose run_hello_world_test)
	mkdir BUILD/build-gcc-$(CROSSARCH)/results.$(DEJAGNU_TIMESTAMP)
	$(MAKE) \
	    -C BUILD/build-gcc-$(CROSSARCH) \
	    DEJAGNU=$(abspath dejagnu/site.exp) \
	    RUNTESTFLAGS=" \
	        --target_board=nacl \
	        --outdir=$(abspath BUILD/build-gcc-$(CROSSARCH)/results.$(DEJAGNU_TIMESTAMP)) \
	        SIM=$(abspath ../scons-out/dbg-linux-x86-$(BITSPLATFORM)/staging/sel_ldr)" \
	    LDFLAGS_FOR_TARGET="-lnosys" \
	    check


##################################################################
# Compile Native Client tests with the toolchain and run them.
##################################################################
nacl-check: ALWAYS
	(cd .. && \
	  ./$(SCONS) -k \
	    $(SCONS_DESTINATIONS) \
	    --mode=dbg-linux,nacl platform=x86-$(BITSPLATFORM) \
	    sdl=none --nacl_glibc --verbose small_tests)


##################################################################
# Remove the BUILD directory.
# Library builds are maintained by scons.
##################################################################
clean:
	-rm -rf BUILD

.PHONY: ALWAYS
