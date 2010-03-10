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
DESTDIR?=
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
      grep '\(/usr/\|\)bin/.*dll' | tr -d \`printf '\r'\` | sort -u` \
      $(DESTDIR)$(SDKROOT)/libexec
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
BINUTILS_VERSION=2.20
NEWLIB_VERSION=1.17.0
GCC_VERSION=4.2.2
GMP_VERSION=4.3.1
MPFR_VERSION=2.4.1
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
	cd BUILD && tar xf $(SRCDIR)/binutils/binutils-$(BINUTILS_VERSION).tar.bz2
	cd BUILD && patch -p0 < ../patches/binutils-$(BINUTILS_VERSION).patch
	if ( [ ! -x /usr/bin/bison ] ) || ( [ ! -x /usr/bin/flex ] ) ; then \
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
	if [ "x$(GCC_VERSION)" != "x4.2.2" ] ; then \
	  cd BUILD && tar xf $(SRCDIR)/gcc/gcc-fortran-$(GCC_VERSION).tar.bz2 ; \
	fi
	if [ "x$(GCC_VERSION)" = "x4.4.2" -o "x$(GCC_VERSION)" = "x4.4.3" ] ; then \
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
	    cd BUILD && \
	      tar xf $(SRCDIR)/gmp/gmp-$(GMP_VERSION).tar.bz2 && \
	      mv gmp-$(GMP_VERSION) gcc-$(GCC_VERSION)/gmp && \
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
	    cd BUILD && \
	      tar xf $(SRCDIR)/mpfr/mpfr-$(MPFR_VERSION).tar.bz2 && \
	      mv mpfr-$(MPFR_VERSION) gcc-$(GCC_VERSION)/mpfr && \
	    cd .. ; \
	  fi ; \
	fi
	set -e; cd BUILD && \
	  for i in ../patches/???-gcc-$(GCC_VERSION).patch ; do \
	    patch -p0 < "$$i" ; \
	  done

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
	$(MAKE) -C BUILD/build-binutils-$(CROSSARCH) DESTDIR=$(DESTDIR) install
	touch $@
	$(MAKE) -f $(THISMAKEFILE) redirector

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
	if [ "x$(GCC_VERSION)" = "x4.4.2" -o "x$(GCC_VERSION)" = "x4.4.3" ] ; then \
	PATH=$(BUILDPATH) \
	  $(MAKE) -C BUILD/build-pregcc-$(CROSSARCH) \
		  CFLAGS_FOR_TARGET="-O2" \
		  CXXFLAGS_FOR_TARGET="-O2" \
	    all-gcc all-target-libgcc ; \
	else \
	PATH=$(BUILDPATH) \
	  $(MAKE) -C BUILD/build-pregcc-$(CROSSARCH) \
		  CFLAGS_FOR_TARGET="-O2" \
		  CXXFLAGS_FOR_TARGET="-O2" \
	    all-gcc ; \
	fi
	if [ "x$(GCC_VERSION)" = "x4.4.2" -o "x$(GCC_VERSION)" = "x4.4.3" ] ; then \
	PATH=$(BUILDPATH) \
	  $(MAKE) -C BUILD/build-pregcc-$(CROSSARCH) \
		  CFLAGS_FOR_TARGET="-O2" \
		  CXXFLAGS_FOR_TARGET="-O2" \
	    DESTDIR=$(DESTDIR) install-gcc install-target-libgcc ; \
	else \
	PATH=$(BUILDPATH) \
	  $(MAKE) -C BUILD/build-pregcc-$(CROSSARCH) \
		  CFLAGS_FOR_TARGET="-O2" \
		  CXXFLAGS_FOR_TARGET="-O2" \
	    DESTDIR=$(DESTDIR) install-gcc ; \
	fi
	cp $(DESTDIR)$(SDKROOT)/lib/gcc/$(CROSSARCH)/$(GCC_VERSION)/libgcc.a \
		$(DESTDIR)$(SDKROOT)/lib/gcc/$(CROSSARCH)/$(GCC_VERSION)/libgcc_eh.a
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
BUILD/stamp-newlib: BUILD/stamp-newlib-$(NEWLIB_VERSION) Makefile
	rm -rf BUILD/build-newlib-$(CROSSARCH)
	mkdir BUILD/build-newlib-$(CROSSARCH)
	../src/trusted/service_runtime/export_header.py ../src/trusted/service_runtime/include \
		BUILD/newlib-$(NEWLIB_VERSION)/newlib/libc/sys/nacl
	if [ "x$(CROSSARCH)" = "xnacl64" ] ; then \
	  cd BUILD/build-newlib-$(CROSSARCH) && \
	    PATH=$(BUILDPATH) \
		  CFLAGS="-O2" \
	      ../newlib-$(NEWLIB_VERSION)/configure \
			  --enable-newlib-io-long-long \
			  --enable-newlib-io-c99-formats \
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
	      ../newlib-$(NEWLIB_VERSION)/configure \
			  --enable-newlib-io-long-long \
			  --enable-newlib-io-c99-formats \
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
BUILD/stamp-gcc: BUILD/stamp-gcc-$(GCC_VERSION) Makefile
	rm -rf BUILD/build-gcc-$(CROSSARCH)
	mkdir BUILD/build-gcc-$(CROSSARCH)
ifeq ($(CYGWIN17), 1)
	mkdir BUILD/build-gcc-$(CROSSARCH)/gcc
	cp -af $(DESTDIR)$(SDKROOT)/libexec/cyg*.dll \
	    BUILD/build-gcc-$(CROSSARCH)/gcc
	cp -af $(DESTDIR)$(SDKROOT)/$(CROSSARCH) \
	    BUILD/build-gcc-$(CROSSARCH)/$(CROSSARCH)
endif
	if [ "x$(GCC_VERSION)" != "x4.2.2" ] ; then \
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
	      --target=$(CROSSARCH) ; \
	else \
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
	      --target=$(CROSSARCH) ; \
	fi
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
	if [ "x$(GCC_VERSION)" = "x4.4.3" -a \
	     "x$(CROSSARCH)" = "xnacl64" ] ; then \
	  pushd $(DESTDIR)/$(SDKROOT) ; \
	  ln -sfn lib/32 lib32 ; \
	  ln -sfn lib lib64 ; \
	  popd ; \
	fi
	touch $@
	$(MAKE) -f $(THISMAKEFILE) redirector

.PHONY: gcc
gcc: BUILD/stamp-gcc

##################################################################
# gdb:
# Builds gdb.
##################################################################
# TODO: Add 64-bit support.
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
	  targetplatform=x86-$(subst nacl,,$(CROSSARCH))

nacl_libraries_nocpp: ALWAYS
	if [ "x$(GCC_VERSION)" = "x4.4.3" -a \
	     "x$(CROSSARCH)" = "xnacl64" ] ; then \
	  cd .. && \
	  (./$(SCONS) naclsdk_mode=custom:$(SDKLOC_NATIVE)/$(SDKNAME) \
	    --mode=nacl_extra_sdk extra_sdk_update_header \
	    nocpp=yes multilib=true --verbose ) && \
	  (./$(SCONS) naclsdk_mode=custom:$(SDKLOC_NATIVE)/$(SDKNAME) \
	    --mode=nacl_extra_sdk extra_sdk_update \
	    nocpp=yes multilib=true targetplatform=x86-32 --verbose ) && \
	  (./$(SCONS) naclsdk_mode=custom:$(SDKLOC_NATIVE)/$(SDKNAME) \
	    --mode=nacl_extra_sdk extra_sdk_update \
	    nocpp=yes multilib=true targetplatform=x86-64 --verbose ) ; \
	else \
	  cd .. && \
	   ./$(SCONS) naclsdk_mode=custom:$(SDKLOC_NATIVE)/$(SDKNAME) \
	   targetplatform=x86-$(subst nacl,,$(CROSSARCH)) \
	   --mode=nacl_extra_sdk extra_sdk_update_header nocpp=yes --verbose ; \
	   ./$(SCONS) naclsdk_mode=custom:$(SDKLOC_NATIVE)/$(SDKNAME) \
	   --mode=nacl_extra_sdk extra_sdk_update nocpp=yes --verbose \
	   targetplatform=x86-$(subst nacl,,$(CROSSARCH)) ; \
	fi

nacl_libraries: ALWAYS
	if [ "x$(GCC_VERSION)" = "x4.4.3" -a \
	     "x$(CROSSARCH)" = "xnacl64" ] ; then \
	  cd .. && \
	  (./$(SCONS) naclsdk_mode=custom:$(SDKLOC_NATIVE)/$(SDKNAME) \
	    --mode=nacl_extra_sdk extra_sdk_update_header multilib=true \
	    --verbose ) && \
	  (./$(SCONS) naclsdk_mode=custom:$(SDKLOC_NATIVE)/$(SDKNAME) \
	    --mode=nacl_extra_sdk extra_sdk_update multilib=true \
	    targetplatform=x86-32 --verbose ) && \
	  (./$(SCONS) naclsdk_mode=custom:$(SDKLOC_NATIVE)/$(SDKNAME) \
	    --mode=nacl_extra_sdk extra_sdk_update multilib=true \
	    targetplatform=x86-64 --verbose ) ; \
	else \
	  cd .. && \
	   (./$(SCONS) naclsdk_mode=custom:$(SDKLOC_NATIVE)/$(SDKNAME) \
	     targetplatform=x86-$(subst nacl,,$(CROSSARCH)) \
	     --mode=nacl_extra_sdk extra_sdk_update_header --verbose) && \
	   (./$(SCONS) naclsdk_mode=custom:$(SDKLOC_NATIVE)/$(SDKNAME) \
	     targetplatform=x86-$(subst nacl,,$(CROSSARCH)) \
	     --mode=nacl_extra_sdk extra_sdk_update --verbose) ; \
	fi

sdk_minimal_test: ALWAYS
	cd .. && \
	 ./$(SCONS) naclsdk_mode=custom:$(SDKLOC_NATIVE)/$(SDKNAME) \
	  --mode=nacl sdk_minimal_test \
	  targetplatform=x86-$(subst nacl,,$(CROSSARCH)) --verbose


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
	done && \
	for exe in "$(DESTDIR)$(SDKROOT)/$(CROSSARCH)/bin/"*.exe ; do \
	  echo "The next command can hang on FAT partiton; use NTFS!" ; \
	  ln -fn redirector.exe $$exe ; \
	done && \
	LEDIR="$(DESTDIR)$(SDKROOT)/libexec" \
	ARCHDIR="$$LEDIR/gcc/$(CROSSARCH)/$(GCC_VERSION)" \
	if [ -d "$$ARCHDIR" ] ; then \
	  for exe in "$$ARCHDIR/"*.exe ; do \
	    if [ ! -L "$$exe" ] ; then \
	      mv "$$exe" \
		"$$LEDIR/$(CROSSARCH)-$(GCC_VERSION)-`basename $$exe`" ; \
	      ln -sfn "../../../$(CROSSARCH)-$(GCC_VERSION)-`basename $$exe`" \
		"$$exe" ; \
	    fi ; \
	  done ; \
	fi
else
	for exe in "$(DESTDIR)$(SDKROOT)/$(CROSSARCH)/bin/"* ; do \
	  ln -sfn "../../bin/$(CROSSARCH)-`basename $$exe`" "$$exe" ; \
	done
endif
	$(CYGWIN_DLL_COPY)

##################################################################
# Build the entire sdk
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
	# gdb only works with nacl, not with nacl64... yet
ifeq ($(CROSSARCH), nacl)
	case $$(BUILD/binutils-$(BINUTILS_VERSION)/config.guess) in \
	*-linux*) $(MAKE) -f $(THISMAKEFILE) BUILD/stamp-gdb ;; \
	esac
endif
	$(MAKE) -f $(THISMAKEFILE) nacl_libraries
	# Don't run tests till loader is not ready
ifeq ($(CROSSARCH), nacl)
	$(MAKE) -f $(THISMAKEFILE) sdk_minimal_test
endif
	$(MAKE) -f $(THISMAKEFILE) redirector

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
TARBALL_FILE=$(shell pwd)/BUILD/$(SDKNAME).tar
tarball:
	(cd $(SDKLOC); tar -cvhf $(TARBALL_FILE) $(SDKNAME); gzip $(TARBALL_FILE))

##################################################################
# Remove the BUILD directory and PRISTINE versions used for patches.
# Library builds are maintained by scons.
##################################################################
clean:
	-rm -rf BUILD PRISTINE

.PHONY: ALWAYS
