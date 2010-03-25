#!/bin/bash
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

# This script builds a NaCl-compatible valgrind binary (memceck-only).
# The binary is packed into a self-contained .sh script.

# Take this specific version of valgrind and VEX.
VALGRIND_REV=11096
VEX_REV=1964

# Locate the patch file.
PATCH_DIR=`dirname $0`
PATCH_DIR=`cd $PATCH_DIR && pwd`
PATCH_FILE=$PATCH_DIR/patches/valgrind-$VALGRIND_REV-VEX-$VEX_REV.patch

# Checkout valgrind
get_valgrind() {
  svn co -r$VALGRIND_REV svn://svn.valgrind.org/valgrind/trunk
  # Make sure we are at specific revision of VEX.
  (cd trunk/VEX && svn up -r$VEX_REV)
}

patch_valgrind() {
  echo "*** Using patch file $PATCH_FILE ***"
  patch -p 0 < $PATCH_FILE
}

build_valgrind() {
  echo "*** BUILDING VALGRIND ***"
  # make sure to build with vanilla system gcc.
  export CC=/usr/bin/gcc
  export CXX=/usr/bin/g++
  # Build 64-bit-only valgrind since 32-bit does not work on NaCl anyway.
  ./autogen.sh && \
  ./configure --enable-only64bit --prefix=`pwd`/inst && \
  make -j 8 && \
  make install &&
  strip $TMP_BUILD_DIR/trunk/inst/lib/valgrind/memcheck-amd64-linux
}

pack_memcheck() {
  # Pack self-contained binary using the script from ThreadSanitizer.
  # TODO(kcc): do we need to have a local copy of mk-self-contained-valgrind.sh?
  wget \
  http://data-race-test.googlecode.com/svn-history/r1882/trunk/tsan_binary/mk-self-contained-valgrind.sh
  chmod +x mk-self-contained-valgrind.sh
  ./mk-self-contained-valgrind.sh $TMP_BUILD_DIR/trunk/inst \
    memcheck $TMP_BUILD_DIR/memcheck.sh
}

# Build somewhere in /tmp so that the built binaries
# don't have your path compiled in.
TMP_BUILD_DIR=/tmp/memcheck_for_nacl

rm -rf $TMP_BUILD_DIR
mkdir -p $TMP_BUILD_DIR
cd $TMP_BUILD_DIR
get_valgrind
cd trunk
patch_valgrind
build_valgrind
pack_memcheck
