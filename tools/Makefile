# Copyright 2008-2011 The Native Client Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can
# be found in the LICENSE file.

##############################################################################
# Helper script for NaCl toolchain development workflow
#
# Buildbots:
# - Sync needed sources at pinned revision and build newlib-based toolchain:
#     make build SDKLOC=<where-to-install-the-toolchain>
#
# Development:
# - Sync all sources at pinned revision:
#     make sync-pinned
# - Sync all sources at most recent revision:
#     make sync
# - Build newlib-based toolchain from current sources:
#     make dev-build SDKLOC=<where-to-install-the-toolchain>
# - Build glibc-based toolchain from current sources:
#     make dev-build-with-glibc SDKLOC=<where-to-install-the-toolchain>
#
# Gotchas:
# - Multilib glibc-based toolchain is not supported. Add
#     MULTILIB=no
#   to make command line when building glibc-based toolchain
#
##############################################################################

default: build

# Delete the target file if the recipe fails after beginning to change the file
# http://www.gnu.org/software/make/manual/make.html#Errors (Errors in Recipes)
.DELETE_ON_ERROR: ;

THISMAKEFILE := $(lastword $(MAKEFILE_LIST))

SHELL=/bin/bash

# By default, checkout from read-only repository:
#   http://git.chromium.org/git
# Maintainers can either override this with read-write repository:
#   ssh://git.chromium.org
# or add to git config:
#   [url "ssh://git.chromium.org"]
#     pushinsteadof = http://git.chromium.org/git
# or add to git-cl codereview.settings:
#   PUSH_URL_CONFIG: url.ssh://git.chromium.org.pushinsteadof
#   ORIGIN_URL_CONFIG: http://git.chromium.org/git
GIT_BASE_URL=http://git.chromium.org/git

CROSSARCH=nacl64
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
      "$(HPREFIX)/sys/audio_video.h" \
      "$(HPREFIX)/sys/nacl_imc_api.h" \
      "$(HPREFIX)/sys/nacl_name_service.h" \
      "$(HPREFIX)/sys/nacl_syscalls.h"
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
# Get or update the sources
##################################################################

SRC:
	mkdir SRC


SRC/binutils: | SRC
	git clone $(GIT_BASE_URL)/nacl-binutils SRC/binutils

.PHONY: fetched-src-binutils
fetched-src-binutils: | SRC/binutils
	cd SRC/binutils && git fetch

.PHONY: pinned-src-binutils
pinned-src-binutils: fetched-src-binutils
	cd SRC/binutils && git checkout "$(NACL_BINUTILS_COMMIT)"

.PHONY: latest-src-binutils
latest-src-binutils: fetched-src-binutils
	./update_to_latest.sh SRC/binutils


SRC/newlib: | SRC
	git clone $(GIT_BASE_URL)/nacl-newlib SRC/newlib

.PHONY: fetched-src-newlib
fetched-src-newlib: | SRC/newlib
	cd SRC/newlib && git fetch

.PHONY: pinned-src-newlib
pinned-src-newlib: fetched-src-newlib
	cd SRC/newlib && git checkout "$(NACL_NEWLIB_COMMIT)"

.PHONY: latest-src-newlib
latest-src-newlib: fetched-src-newlib
	./update_to_latest.sh SRC/newlib


SRC/gcc: | SRC
	git clone $(GIT_BASE_URL)/nacl-gcc SRC/gcc

.PHONY: fetched-src-gcc
fetched-src-gcc: | SRC/gcc
	cd SRC/gcc && git fetch

.PHONY: pinned-src-gcc
pinned-src-gcc: fetched-src-gcc
	cd SRC/gcc && git checkout "$(NACL_GCC_COMMIT)"
	./use_gcc_extras.sh GMP_VERSION=$(GMP_VERSION) MPFR_VERSION=$(MPFR_VERSION)

.PHONY: latest-src-gcc
latest-src-gcc: fetched-src-gcc
	./update_to_latest.sh SRC/gcc
	./use_gcc_extras.sh GMP_VERSION=$(GMP_VERSION) MPFR_VERSION=$(MPFR_VERSION)


SRC/glibc: | SRC
	git clone $(GIT_BASE_URL)/nacl-glibc.git SRC/glibc

.PHONY: fetched-src-glibc
fetched-src-glibc: | SRC/glibc
	cd SRC/glibc && git fetch

.PHONY: pinned-src-glibc
pinned-src-glibc: fetched-src-glibc
	cd SRC/glibc && git checkout "$(NACL_GLIBC_COMMIT)"

.PHONY: latest-src-glibc
latest-src-glibc: fetched-src-glibc
	./update_to_latest.sh SRC/glibc


SRC/linux-headers-for-nacl: | SRC
	git clone $(GIT_BASE_URL)/linux-headers-for-nacl.git \
	  SRC/linux-headers-for-nacl

.PHONY: fetched-src-linux-headers-for-nacl
fetched-src-linux-headers-for-nacl: | SRC/linux-headers-for-nacl
	cd SRC/linux-headers-for-nacl && git fetch

.PHONY: pinned-src-linux-headers-for-nacl
pinned-src-linux-headers-for-nacl: fetched-src-linux-headers-for-nacl
	cd SRC/linux-headers-for-nacl && \
	  git checkout "$(LINUX_HEADERS_FOR_NACL_COMMIT)"

.PHONY: latest-src-linux-headers-for-nacl
latest-src-linux-headers-for-nacl: fetched-src-linux-headers-for-nacl
	./update_to_latest.sh SRC/linux-headers-for-nacl


SRC/gdb: | SRC
	git clone $(GIT_BASE_URL)/nacl-gdb SRC/gdb

.PHONY: fetched-src-gdb
fetched-src-gdb: | SRC/gdb
	cd SRC/gdb && git fetch

.PHONY: pinned-src-gdb
pinned-src-gdb: fetched-src-gdb
	cd SRC/gdb && git checkout "$(NACL_GDB_COMMIT)"

.PHONY: latest-src-gdb
latest-src-gdb: fetched-src-gdb
	./update_to_latest.sh SRC/gdb


.PHONY: sync-pinned
sync-pinned: | \
  pinned-src-binutils \
  pinned-src-gcc \
  pinned-src-newlib \
  pinned-src-glibc \
  pinned-src-linux-headers-for-nacl \
  pinned-src-gdb ;

.PHONY: sync
sync: | \
  latest-src-binutils \
  latest-src-gcc \
  latest-src-newlib \
  latest-src-glibc \
  latest-src-linux-headers-for-nacl \
  latest-src-gdb ;


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

GLIBC_SDKDIRS=\
    $(DESTDIR)$(SDKROOT)/bin \
    $(DESTDIR)$(SDKROOT)/include \
    $(DESTDIR)$(SDKROOT)/lib \
    $(DESTDIR)$(SDKROOT)/libexec \
    $(DESTDIR)$(SDKROOT)/nacl \
    $(DESTDIR)$(SDKROOT)/nacl64/lib \
    $(DESTDIR)$(SDKROOT)/nacl64/lib32

# This structure has symlinks in toolchain/nacl64 subdirectory. This causes
# problems on Windows buildbots running non-Cygwin python (scons).
# TODO(pasko): either switch to another link structure or run scons in Cygwin on
# Windows.
.PHONY: glibc-sdkdirs
glibc-sdkdirs:
	echo "Creating the SDK tree at $(DESTDIR)$(SDKROOT)"
	@for d in $(GLIBC_SDKDIRS); do \
	  if [ ! -d $$d ]; then \
	    echo install -m 755 -d $$d; \
	    install -m 755 -d $$d; \
	  fi; \
	done
	# Make sure there is a single install location for all header files.
	ln -sfn ../include $(DESTDIR)$(SDKROOT)/nacl/include
	ln -sfn ../include $(DESTDIR)$(SDKROOT)/nacl64/include
	# libgcc_s.so is installed to lib64
	ln -sfn lib $(DESTDIR)$(SDKROOT)/nacl64/lib64
	# GCC searches for 32bit libraries in nacl64/lib/32 and in nacl64/lib32
	# at different times.
	ln -sfn ../lib32 $(DESTDIR)$(SDKROOT)/nacl64/lib/32
	ln -sfn ../nacl64/lib32 $(DESTDIR)$(SDKROOT)/nacl/lib

##################################################################
# binutils:
# Builds the cross assembler, linker, archiver, etc.
##################################################################
BUILD/stamp-$(CROSSARCH)-binutils: | SRC/binutils BUILD
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

BUILD/stamp-$(CROSSARCH)-pregcc: | SRC/gcc BUILD
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
BUILD/stamp-$(CROSSARCH)-pregcc-standalone: \
  BUILD/stamp-$(CROSSARCH)-binutils | SRC/gcc BUILD
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
BUILD/stamp-$(CROSSARCH)-newlib: | SRC/newlib BUILD
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

# CFLAGS for building glibc.
GLIBC_CFLAGS=-O2 -g

# LIB_BITS is used with different values to execute targets in this Makefile for
# different architectures (32, 64) when building libraries (glibc and nacl).
# CROSSARCH and BITSPLATFORM could be used for this, but we better avoid
# redefining variables with recursive $(MAKE) calls.
LIB_BITS?=64
ARCH_DEST=$(DESTDIR)$(SDKROOT)/$(if $(filter 32,$(LIB_BITS)),nacl,nacl64)

BUILD/stamp-glibc32: BUILD/stamp-$(CROSSARCH)-pregcc-standalone \
  | SRC/glibc SRC/linux-headers-for-nacl
	rm -rf BUILD/build-glibc32
	mkdir -p BUILD/build-glibc32/lib
	cd BUILD/build-glibc32 && ../../SRC/glibc/configure \
	    BUILD_CC="gcc -O2 -g" \
	    CC="$(GLIBC_CC) -m32" \
	    CFLAGS="$(GLIBC_CFLAGS) -pipe -fno-strict-aliasing -mno-tls-direct-seg-refs -march=i486" \
	    libc_cv_forced_unwind=yes \
	    libc_cv_c_cleanup=yes \
	    libc_cv_slibdir=$(PREFIX)/nacl64/lib32 \
	    --prefix=$(PREFIX)/nacl \
	    --host=i486-linux-gnu \
	    --with-headers=$(LINUX_HEADERS) \
	    --enable-kernel=2.2.0
	$(MAKE) -C BUILD/build-glibc32
	$(MAKE) -C BUILD/build-glibc32 DESTDIR=$(DESTDIR) install
	touch $@

BUILD/stamp-glibc64: BUILD/stamp-$(CROSSARCH)-pregcc-standalone \
  | SRC/glibc SRC/linux-headers-for-nacl
	rm -rf BUILD/build-glibc64
	mkdir -p BUILD/build-glibc64
	cd BUILD/build-glibc64 && ../../SRC/glibc/configure \
	    BUILD_CC="gcc -O2 -g" \
	    CC="$(GLIBC_CC) -m64" \
	    CFLAGS="$(GLIBC_CFLAGS) -pipe -fno-strict-aliasing -mno-tls-direct-seg-refs" \
	    libc_cv_forced_unwind=yes \
	    libc_cv_c_cleanup=yes \
	    libc_cv_slibdir=$(PREFIX)/nacl64/lib \
	    --prefix=$(PREFIX)/nacl64 \
	    --host=x86_64-linux-gnu \
	    --with-headers=$(LINUX_HEADERS) \
	    --enable-kernel=2.2.0
	$(MAKE) -C BUILD/build-glibc64
	$(MAKE) -C BUILD/build-glibc64 DESTDIR=$(DESTDIR) install
	touch $@

.PHONY: export-headers
export-headers:
	rm -rf $(HPREFIX)
	../src/trusted/service_runtime/export_header.py \
	  ../src/trusted/service_runtime/include $(HPREFIX)

##################################################################
# Ad hoc linker scripts and a selection of NaCl headers for GCC.
##################################################################
.PHONY: glibc-adhoc-files
glibc-adhoc-files:
	install -m 755 -d  $(ARCH_DEST)/lib/ldscripts
	cp -f SRC/glibc/nacl/dyn-link/ldscripts/* \
	    $(ARCH_DEST)/lib/ldscripts/
	cp -rf $(LINUX_HEADERS)/* $(ARCH_DEST)/include
	mkdir -p $(ARCH_DEST)/include/{sys,machine}
	cp -f $(NACL_BITS_HEADERS) $(ARCH_DEST)/include/bits
	cp -f $(NACL_SYS_HEADERS) $(ARCH_DEST)/include/sys
	cp -f $(NACL_MACHINE_HEADERS) $(ARCH_DEST)/include/machine
	cp ../src/untrusted/include/machine/_default_types.h \
	    $(ARCH_DEST)/include/machine/_default_types.h
	echo '/* Intentionally empty */' > \
	    $(ARCH_DEST)/lib/libnacl.so
	echo '/* Intentionally empty */' > \
	    $(ARCH_DEST)/lib/libcrt_platform.a
	echo '/* Intentionally empty */' > \
	    $(ARCH_DEST)/lib/libnosys.so
	for f in catchsegv gencat getconf getent iconv ldd locale \
	    localedef mtrace pcprofiledump rpcgen sprof tzselect xtrace; do \
	    rm -f $(ARCH_DEST)/bin/$$f ; \
	done


##################################################################
# Native Client libraries.
##################################################################

# Extra arguments to scons to define build and install destinations.
SCONS_DESTINATIONS_NOLIB=\
 DESTINATION_ROOT=tools/BUILD/build-extra_sdk-$(CROSSARCH) \
 naclsdk_mode=custom:$(SDKLOC_NATIVE)/$(SDKNAME) \
 extra_sdk_include_destination=$(DESTDIR)$(SDKROOT)/$(CROSSARCH)/include \

BUILD_EXTRA_SDK=BUILD/build-extra_sdk-$(CROSSARCH)

.PHONY: scons-libs-glibc
scons-libs-glibc: glibc-adhoc-files
	rm -rf $(BUILD_EXTRA_SDK)
	mkdir $(BUILD_EXTRA_SDK)
	cd .. && \
	  (./$(SCONS) \
	    $(SCONS_DESTINATIONS_NOLIB) \
	    extra_sdk_lib_destination=$(ARCH_DEST)/lib \
	    MODE=nacl_extra_sdk --nacl_glibc \
	    extra_sdk_update_header extra_sdk_update \
	    targetplatform=x86-$(LIB_BITS) --verbose )
	# Remove the fake libnacl.so to let libnacl.a be used to run tests.
	if [ -f $(ARCH_DEST)/lib/libnacl.a ]; then \
	  rm -f $(ARCH_DEST)/lib/libnacl.so; \
	fi

NCVAL_STUBOUT=$(abspath $(BUILD_EXTRA_SDK)/opt-linux-x86-$(LIB_BITS)/staging/ncval_stubout)
.PHONY: stubout
stubout:
	cd .. && ./$(SCONS) --mode=opt-linux platform=x86-$(LIB_BITS) \
	    $(SCONS_DESTINATIONS_NOLIB) ncval_stubout
	set -e; find $(ARCH_DEST)/lib/ -name '*.so' | \
	  while read library ; do \
	    if [ "$$(head -c 4 "$$library" | tail -c 3)" = 'ELF' ] ; then \
	      echo $(NCVAL_STUBOUT) "$$library" -o "$$library"; \
	      $(NCVAL_STUBOUT) "$$library" -o "$$library"; \
	    fi \
	  done


##################################################################
# gcc:
#   Builds GCC with glibc as a C library.
##################################################################
BUILD/stamp-$(CROSSARCH)-full-gcc: glibc-adhoc-files
	rm -rf BUILD/build-full-gcc-$(CROSSARCH)
	mkdir BUILD/build-full-gcc-$(CROSSARCH)
	# See http://code.google.com/p/nativeclient/issues/detail?id=854
	rm -rf $(DESTDIR)$(SDKROOT)/$(CROSSARCH)/sys-include
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
BUILD/stamp-$(CROSSARCH)-gcc: | SRC/gcc BUILD
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
	$(MAKE) -f $(THISMAKEFILE) gcc-redirector
	touch $@

.PHONY: gcc-redirector
gcc-redirector:
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

.PHONY: gcc
gcc: BUILD/stamp-$(CROSSARCH)-gcc

##################################################################
# gdb:
# Builds gdb.
##################################################################
# TODO: Add 64-bit support.
# Only linux is supported.
BUILD/stamp-$(CROSSARCH)-gdb: | SRC/gdb BUILD
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

.PHONY: dev-build-with-glibc
dev-build-with-glibc:
	$(MAKE) -f $(THISMAKEFILE) glibc-sdkdirs
	cp -f SRC/gcc/COPYING* $(DESTDIR)$(SDKROOT)
	$(MAKE) -f $(THISMAKEFILE) BUILD/stamp-$(CROSSARCH)-binutils
	$(MAKE) -f $(THISMAKEFILE) BUILD/stamp-$(CROSSARCH)-pregcc-standalone
	$(MAKE) -f $(THISMAKEFILE) BUILD/stamp-glibc32
	$(MAKE) -f $(THISMAKEFILE) BUILD/stamp-glibc64
	$(MAKE) -f $(THISMAKEFILE) export-headers
	$(MAKE) -f $(THISMAKEFILE) glibc-adhoc-files LIB_BITS=32
	$(MAKE) -f $(THISMAKEFILE) glibc-adhoc-files LIB_BITS=64
	$(MAKE) -f $(THISMAKEFILE) BUILD/stamp-$(CROSSARCH)-full-gcc
	$(MAKE) -f $(THISMAKEFILE) scons-libs-glibc LIB_BITS=32
	$(MAKE) -f $(THISMAKEFILE) scons-libs-glibc LIB_BITS=64
	$(MAKE) -f $(THISMAKEFILE) stubout LIB_BITS=32
	$(MAKE) -f $(THISMAKEFILE) stubout LIB_BITS=64
	$(MAKE) -f $(THISMAKEFILE) redirector
	$(MAKE) -f $(THISMAKEFILE) gcc-redirector

.PHONY: dev-build
dev-build:
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

# Newlib toolchain for buildbot.
.PHONY: build
build: | \
  pinned-src-binutils \
  pinned-src-gcc \
  pinned-src-newlib \
  pinned-src-gdb
	$(MAKE) -f $(THISMAKEFILE) dev-build

# Glibc toolchain for buildbot.
.PHONY: buildbot-build-with-glibc
buildbot-build-with-glibc: | \
  pinned-src-binutils \
  pinned-src-gcc \
  pinned-src-glibc \
  pinned-src-linux-headers-for-nacl
	$(MAKE) -f $(THISMAKEFILE) dev-build-with-glibc

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
	    $(SCONS_DESTINATIONS_NOLIB) \
	    --mode=dbg-linux,nacl platform=x86-$(BITSPLATFORM) \
	    sdl=none --nacl_glibc --verbose small_tests)


##################################################################
# Remove the BUILD directory.
# Library builds are maintained by scons.
##################################################################

.PHONY: clean
clean:
	rm -rf BUILD

.PHONY: ALWAYS
