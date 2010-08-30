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
#      make SDKLOC=/your/path clean build
#    Option 2: Build by copying the sources from the Git repository:
#      git clone http://git.chromium.org/git/nacl-toolchain.git SRC
#      make prepare-git-src
#      make SDKLOC=/your/path clean build
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
TOOLCHAINLOC?=/usr/local
SDKLOC?=$(TOOLCHAINLOC)
SDKNAME=nacl-sdk
SDKROOT=$(SDKLOC)/$(SDKNAME)
DESTDIR?=
BUILDNCCDIR=$(SDKROOT)/$(CROSSARCH)

# Convert CROSSARCH (nacl or nacl64) to (32 or 64).
BITSPLATFORM=$(subst 3264,64,$(subst nacl,32,$(CROSSARCH)))

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
BINUTILS_VERSION=2.20.1
NEWLIB_VERSION=1.18.0
GCC_VERSION=4.4.3
GMP_VERSION=4.3.1
MPFR_VERSION=2.4.1
GDB_VERSION=6.8

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
	$(MAKE) -f $(THISMAKEFILE) unpack-gcc-extras
	touch SRC/stamp-binutils
	touch SRC/stamp-gcc
	touch SRC/stamp-newlib
	touch SRC/stamp-gdb
	touch stamp-src

# Unpack binutils, apply patches.
SRC/binutils: | SRC
	@echo "Unpacking binutils-$(BINUTILS_VERSION)"
	rm -rf SRC/binutils-$(BINUTILS_VERSION)
	cd SRC && tar xf $(SRCDIR)/binutils/binutils-$(BINUTILS_VERSION).tar.bz2
	mv SRC/binutils-$(BINUTILS_VERSION) SRC/binutils
	if ( [ ! -x /usr/bin/bison ] ) || ( [ ! -x /usr/bin/flex ] ) ; then \
	  cd SRC/binutils && ( patch -p1 < ../../patches/binutils-$(BINUTILS_VERSION).ly.patch || true ) ; \
	fi
	printf -- "--- SRC/binutils/bfd/version.h\n\
	+++ SRC/binutils/bfd/version.h\n\
	@@ -3 +3 @@\n\
	-#define BFD_VERSION_STRING  @bfd_version_package@ @bfd_version_string@\n\
	+#define BFD_VERSION_STRING  @bfd_version_package@ @bfd_version_string@ \" `svn info | grep 'Last Changed Date' | sed -e s'+Last Changed Date: \(....\)-\(..\)-\(..\).*+\1\2\3+'` (Native Client r`svn info | grep 'Last Changed Rev' | sed -e s'+Last Changed Rev: ++'`)\"\n" |\
	patch -p0
	set -e; cd SRC && \
	  for i in ../patches/binutils-$(BINUTILS_VERSION)/????-*.patch ; do \
	    patch -p1 < "$$i" ; \
	  done

# Create binutils source directory, and mark that we've done that.
SRC/stamp-binutils: | SRC
	rm -rf SRC/binutils
	$(MAKE) -f $(THISMAKEFILE) SRC/binutils
	touch $@

# Unpack newlib, apply patches.
SRC/newlib: | SRC
	@echo "Unpacking newlib-$(NEWLIB_VERSION)"
	rm -rf SRC/newlib-$(NEWLIB_VERSION)
	cd SRC && tar xf $(SRCDIR)/newlib/newlib-$(NEWLIB_VERSION).tar.gz
	mv SRC/newlib-$(NEWLIB_VERSION) SRC/newlib
	set -e; cd SRC && \
	  for i in ../patches/newlib-$(NEWLIB_VERSION)/????-*.patch ; do \
	    patch -p1 < "$$i" ; \
	  done

# Create newlib source directory, and mark that we've done that.
SRC/stamp-newlib: | SRC
	rm -rf SRC/newlib
	$(MAKE) -f $(THISMAKEFILE) SRC/newlib
	touch $@

# Unpack gcc, apply patches.
SRC/gcc: | SRC
	@echo "Unpacking gcc-$(GCC_VERSION)"
	rm -rf SRC/gcc-$(GCC_VERSION)
	cd SRC && tar xf $(SRCDIR)/gcc/gcc-core-$(GCC_VERSION).tar.bz2
	cd SRC && tar xf $(SRCDIR)/gcc/gcc-g++-$(GCC_VERSION).tar.bz2
	cd SRC && tar xf $(SRCDIR)/gcc/gcc-fortran-$(GCC_VERSION).tar.bz2
	cd SRC && tar xf $(SRCDIR)/gcc/gcc-testsuite-$(GCC_VERSION).tar.bz2
	mv SRC/gcc-$(GCC_VERSION) SRC/gcc
	$(MAKE) -f $(THISMAKEFILE) unpack-gcc-extras
	printf -- "`svn info | grep 'Last Changed Date' | sed -e s'+Last Changed Date: \(....\)-\(..\)-\(..\).*+\1\2\3+'`" > SRC/gcc/gcc/DATESTAMP
	printf -- "Native Client r`svn info | grep 'Last Changed Rev' | sed -e s'+Last Changed Rev: ++'`" > SRC/gcc/gcc/DEV-PHASE
	set -e; cd SRC && \
	  for i in ../patches/gcc-$(GCC_VERSION)/????-*.patch ; do \
	    patch -p1 < "$$i" ; \
	  done

# Create gcc source directory, and mark that we've done that.
SRC/stamp-gcc: | SRC
	rm -rf SRC/gcc
	$(MAKE) -f $(THISMAKEFILE) SRC/gcc
	touch $@

# Unpack GCC libs (GMP, MPFR) from tarballs if they are not in the system.
unpack-gcc-extras: ALWAYS | SRC
	GNU_MP_MAJOR=0 ; \
	if [ -f /usr/include/gmp.h ] ; then \
	  GNU_MP_MAJOR="`grep '#[ 	]*define[ 	]*__GNU_MP_VERSION ' /usr/include/gmp.h | sed -e s'@[ 	]*#define[ 	]*__GNU_MP_VERSION[ 	]*@@'`" ; \
	  GNU_MP_MINOR="`grep '#[ 	]*define[ 	]*__GNU_MP_VERSION_MINOR ' /usr/include/gmp.h | sed -e s'@[ 	]*#[ 	]*define[ 	]*__GNU_MP_VERSION_MINOR[ 	]*@@'`" ; \
	  GNU_MP_PATCHLEVEL="`grep '#[ 	]*define[ 	]*__GNU_MP_VERSION_PATCHLEVEL ' /usr/include/gmp.h | sed -e s'@[ 	]*[ 	]*#define[ 	]*__GNU_MP_VERSION_PATCHLEVEL[ 	]*@@'`" ; \
	fi ; \
	if [ "$$GNU_MP_MAJOR" -lt 4 ] || \
	   ( [ "$$GNU_MP_MAJOR" -eq 4 ] && \
	     [ "$$GNU_MP_MINOR" -lt 3 ] ) || \
	   ( [ "$$GNU_MP_MAJOR" -eq 4 ] && \
	     [ "$$GNU_MP_MINOR" -eq 3 ] && \
	     [ "$$GNU_MP_PATCHLEVEL" -lt 1 ] ) ; then \
	  cd SRC && \
	    rm -rf gmp-$(GMP_VERSION) && \
	    tar xf $(SRCDIR)/gmp/gmp-$(GMP_VERSION).tar.bz2 && \
	    mv gmp-$(GMP_VERSION) gcc/gmp && \
	    cp gcc/gmp/configure \
	        gcc/gmp/configure.old && \
	    cat gcc/gmp/configure.old | \
	        sed -e 's/M4=m4-not-needed/: # no m4/g' \
	        >gcc/gmp/configure && \
	  cd .. ; \
	fi ; \
	MPFR_VERSION_MAJOR=0 ; \
	if [ -f /usr/include/mpfr.h ] ; then \
	  MPFR_VERSION_MAJOR="`grep '#[ 	]*define[ 	]*MPFR_VERSION_MAJOR ' /usr/include/mpfr.h | sed -e s'@[ 	]*#[ 	]*define[ 	]*MPFR_VERSION_MAJOR[ 	]*@@'`" ; \
	  MPFR_VERSION_MINOR="`grep '#[ 	]*define[ 	]*MPFR_VERSION_MINOR ' /usr/include/mpfr.h | sed -e s'@[ 	]*#[ 	]*define[ 	]*MPFR_VERSION_MINOR[ 	]*@@'`" ; \
	  MPFR_VERSION_PATCHLEVEL="`grep '#[ 	]*define[ 	]*MPFR_VERSION_PATCHLEVEL ' /usr/include/mpfr.h | sed -e s'@[ 	]*#[ 	]*define[ 	]*MPFR_VERSION_PATCHLEVEL[ 	]*@@'`" ; \
	fi ; \
	if [ ! -e /usr/include/mpfr.h ] || \
	   [ "$$MPFR_VERSION_MAJOR" -lt 2 ] || \
	   ( [ "$$MPFR_VERSION_MAJOR" -eq 2 ] && \
	     [ "$$MPFR_VERSION_MINOR" -lt 4 ] ) || \
	   ( [ "$$MPFR_VERSION_MAJOR" -eq 2 ] && \
	     [ "$$MPFR_VERSION_MINOR" -eq 4 ] && \
	     [ "$$MPFR_VERSION_PATCHLEVEL" -lt 1 ] ) ; then \
	  cd SRC && \
	    rm -rf mpfr-$(MPFR_VERSION) && \
	    tar xf $(SRCDIR)/mpfr/mpfr-$(MPFR_VERSION).tar.bz2 && \
	    mv mpfr-$(MPFR_VERSION) gcc/mpfr && \
	  cd .. ; \
	fi ;

# Unpack gdb, apply patches.
SRC/gdb: | SRC
	@echo "Unpacking gdb-$(GDB_VERSION)"
	rm -rf SRC/gdb-$(GDB_VERSION)
	cd SRC && tar xf $(SRCDIR)/gdb/gdb-$(GDB_VERSION).tar.bz2
	mv SRC/gdb-$(GDB_VERSION) SRC/gdb
	printf -- "--- SRC/gdb/gdb/version.in\n\
	+++ SRC/gdb/gdb/version.in\n\
	@@ -1 +1 @@\n\
	-$(GDB_VERSION)\n\
	+$(GDB_VERSION) `svn info | grep 'Last Changed Date' | sed -e s'+Last Changed Date: \(....\)-\(..\)-\(..\).*+\1\2\3+'` (Native Client r`svn info | grep 'Last Changed Rev' | sed -e s'+Last Changed Rev: ++'`)\n" |\
	patch -p0
	set -e; cd SRC && \
	  for i in ../patches/gdb-$(GDB_VERSION)/????-*.patch ; do \
	    patch -p1 < "$$i" ; \
	  done

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
BUILD/stamp-binutils: SRC/stamp-binutils | BUILD
	rm -rf BUILD/build-binutils-$(CROSSARCH)
	mkdir BUILD/build-binutils-$(CROSSARCH)
	if ( [ -x /usr/bin/bison ] ) && ( [ -x /usr/bin/flex ] ) ; then \
	  touch SRC/binutils/ld/ldgram.y ; \
	  touch SRC/binutils/ld/ldlex.l ; \
	fi
	cd BUILD/build-binutils-$(CROSSARCH) && \
	  CC="gcc" \
	  CFLAGS="-m32 -O2 -DNACL_ALIGN_BYTES=$(DEFAULT_ALIGN_BYTES) -DNACL_ALIGN_POW2=$(DEFAULT_ALIGN_POW2)" \
	  LDFLAGS="-s" \
	  ../../SRC/binutils/configure \
	    --prefix=$(SDKROOT) \
	    --target=${CROSSARCH}
	$(MAKE) -C BUILD/build-binutils-$(CROSSARCH) all
	$(MAKE) -C BUILD/build-binutils-$(CROSSARCH) DESTDIR=$(DESTDIR) install
	touch $@
	$(MAKE) -f $(THISMAKEFILE) redirector

.PHONY: binutils
binutils: BUILD/stamp-binutils

##################################################################
# pregcc:
# Builds the cross gcc used to build the libraries.
##################################################################

GCC_CONFIGURE_FLAGS = \
    --disable-decimal-float \
    --disable-libgomp \
    --disable-libmudflap \
    --disable-libssp \
    --disable-libstdcxx-pch \
    --disable-shared \
    --prefix=$(SDKROOT) \
    --target=$(CROSSARCH)

BUILD/stamp-pregcc: SRC/stamp-gcc | BUILD
	rm -rf BUILD/build-pregcc-$(CROSSARCH)
	mkdir BUILD/build-pregcc-$(CROSSARCH)
	cd BUILD/build-pregcc-$(CROSSARCH) && \
	  PATH=$(BUILDPATH) CC="gcc" \
		CFLAGS="-m32 -Dinhibit_libc -D__gthr_posix_h -DNACL_ALIGN_BYTES=$(DEFAULT_ALIGN_BYTES) -DNACL_ALIGN_POW2=$(DEFAULT_ALIGN_POW2)" \
	  LDFLAGS="-s" \
	    ../../SRC/gcc/configure \
	      $(GCC_CONFIGURE_FLAGS) \
	      --disable-threads \
	      --enable-languages="c" \
	      --without-headers
	PATH=$(BUILDPATH) \
	  $(MAKE) -C BUILD/build-pregcc-$(CROSSARCH) \
		  CFLAGS_FOR_TARGET="-O2" \
		  CXXFLAGS_FOR_TARGET="-O2" \
	    all-gcc all-target-libgcc ;
	PATH=$(BUILDPATH) \
	  $(MAKE) -C BUILD/build-pregcc-$(CROSSARCH) \
		  CFLAGS_FOR_TARGET="-O2" \
		  CXXFLAGS_FOR_TARGET="-O2" \
	    DESTDIR=$(DESTDIR) install-gcc install-target-libgcc ; \
	cp $(DESTDIR)$(SDKROOT)/lib/gcc/$(CROSSARCH)/$(GCC_VERSION)/libgcc.a \
		$(DESTDIR)$(SDKROOT)/lib/gcc/$(CROSSARCH)/$(GCC_VERSION)/libgcc_eh.a
	cp $(DESTDIR)$(SDKROOT)/lib/gcc/$(CROSSARCH)/$(GCC_VERSION)/32/libgcc.a \
		$(DESTDIR)$(SDKROOT)/lib/gcc/$(CROSSARCH)/$(GCC_VERSION)/32/libgcc_eh.a |\
	true
	touch $@
	$(MAKE) -f $(THISMAKEFILE) redirector

.PHONY: pregcc
pregcc: BUILD/stamp-pregcc

##################################################################
# newlib:
# Builds the bare-bones library used by NativeClient applications.
# NOTE: removes the default pthread.h to enable correct install
# by the Native Client threads package build.
##################################################################
BUILD/stamp-newlib: SRC/stamp-newlib | BUILD
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
newlib: BUILD/stamp-newlib

##################################################################
# gcc:
# Builds the gcc that will be used to build applications.
##################################################################
BUILD/stamp-gcc: SRC/stamp-gcc | BUILD
	rm -rf BUILD/build-gcc-$(CROSSARCH)
	mkdir BUILD/build-gcc-$(CROSSARCH)
ifeq ($(CYGWIN17), 1)
	mkdir BUILD/build-gcc-$(CROSSARCH)/gcc
	cp -af $(DESTDIR)$(SDKROOT)/libexec/cyg*.dll \
	    BUILD/build-gcc-$(CROSSARCH)/gcc
	cp -af $(DESTDIR)$(SDKROOT)/$(CROSSARCH) \
	    BUILD/build-gcc-$(CROSSARCH)/$(CROSSARCH)
endif
	cd BUILD/build-gcc-$(CROSSARCH) && \
	  PATH=$(BUILDPATH) CC="gcc" \
		CFLAGS="-m32 -Dinhibit_libc -DNACL_ALIGN_BYTES=$(DEFAULT_ALIGN_BYTES) -DNACL_ALIGN_POW2=$(DEFAULT_ALIGN_POW2)" \
		CFLAGS_FOR_TARGET="-O2 -g" \
		CXXFLAGS_FOR_TARGET="-O2 -g" \
	  LDFLAGS="-s" \
	    ../../SRC/gcc/configure \
	      $(GCC_CONFIGURE_FLAGS) \
	      --enable-languages="c,c++" \
	      --enable-threads=nacl \
	      --enable-tls \
	      --with-newlib
	PATH=$(BUILDPATH) \
	  $(MAKE) -C BUILD/build-gcc-$(CROSSARCH) \
		  CFLAGS_FOR_TARGET="-O2 -g" \
		  CXXFLAGS_FOR_TARGET="-O2 -g" \
	    all
	PATH=$(BUILDPATH) \
	  $(MAKE) -C BUILD/build-gcc-$(CROSSARCH) \
		  CFLAGS_FOR_TARGET="-O2 -g" \
		  CXXFLAGS_FOR_TARGET="-O2 -g" \
	    DESTDIR=$(DESTDIR) install
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
gcc: BUILD/stamp-gcc

##################################################################
# gdb:
# Builds gdb.
##################################################################
# TODO: Add 64-bit support.
# Only linux is supported.
BUILD/stamp-gdb: SRC/stamp-gdb | BUILD
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
gdb: BUILD/stamp-gdb

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
	printf -- "--- $(DESTDIR)$(SDKROOT)/$(CROSSARCH)/include/nacl/npapi_extensions.h\n\
	+++ $(DESTDIR)$(SDKROOT)/$(CROSSARCH)/include/nacl/npapi_extensions.h\n\
	@@ -9,3 +9 @@\n\
	-// Use the shorter include path here so that this file can be used in non-\n\
	-// Chromium projects, such as the Native Client SDK.\n\
	-#include \"npapi.h\"\n\
	+#include <nacl/npapi.h>\n\
	--- $(DESTDIR)$(SDKROOT)/$(CROSSARCH)/include/nacl/npruntime.h\n\
	+++ $(DESTDIR)$(SDKROOT)/$(CROSSARCH)/include/nacl/npruntime.h\n\
	@@ -73 +73 @@\n\
	-#include \"npapi.h\"\n\
	+#include <nacl/npapi.h>\n\
	--- $(DESTDIR)$(SDKROOT)/$(CROSSARCH)/include/pgl/pgl.h\n\
	+++ $(DESTDIR)$(SDKROOT)/$(CROSSARCH)/include/pgl/pgl.h\n\
	@@ -8,2 +8,2 @@\n\
	-#include <npapi.h>\n\
	-#include <npapi_extensions.h>\n\
	+#include <nacl/npapi.h>\n\
	+#include <nacl/npapi_extensions.h>\n" |\
	patch -p0 -N

sdk_minimal_test: ALWAYS
	cd .. && \
	   ./$(SCONS) naclsdk_mode=custom:$(SDKLOC_NATIVE)/$(SDKNAME) \
	     --mode=nacl sdk_minimal_test platform=x86-32 --verbose;

gdb_minimal_test: BUILD/stamp-gdb
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

build: ALWAYS
	$(MAKE) -f $(THISMAKEFILE) sdkdirs
	cp -f ../../third_party/gcc/COPYING* $(DESTDIR)$(SDKROOT)
	$(MAKE) -f $(THISMAKEFILE) BUILD/stamp-binutils
	$(MAKE) -f $(THISMAKEFILE) BUILD/stamp-pregcc
	$(MAKE) -f $(THISMAKEFILE) BUILD/stamp-newlib
	$(MAKE) -f $(THISMAKEFILE) nc_threads
	# We need libnacl.a for gfortran, but we don't have a c++ yet
	$(MAKE) -f $(THISMAKEFILE) nacl_libraries_nocpp
	$(MAKE) -f $(THISMAKEFILE) BUILD/stamp-gcc
	case $$(SRC/binutils/config.guess) in \
	*-linux*) $(MAKE) -f $(THISMAKEFILE) BUILD/stamp-gdb ;; \
	esac
	$(MAKE) -f $(THISMAKEFILE) nacl_libraries
	$(MAKE) -f $(THISMAKEFILE) sdk_minimal_test
	$(MAKE) -f $(THISMAKEFILE) redirector
	# We delete posix_over_srpc library until it is good enough for binary
	# distribution.
	rm -f "$(DESTDIR)$(SDKROOT)/$(CROSSARCH)"/lib{,/32}/libposix_over_srpc.a

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
# Remove the BUILD directory.
# Library builds are maintained by scons.
##################################################################
clean:
	-rm -rf BUILD

.PHONY: ALWAYS
