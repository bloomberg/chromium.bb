#!/bin/sh
# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# Script to build binutils found in /build/binutils-XXXX when inside a chroot.
# Don't call this script yourself, instead use the build-all.sh script.

set -e

if [ -z "$1" ]; then
 echo "Directory of binutils not given."
 exit 1
fi

cd "$1"

# Ask the dynamic loader to load libstdc++ from the LLVM build directory if
# available. That copy of libstdc++ is required by the gold plugin in the same
# directory. The dynamic loader expects the relative path to start with $ORIGIN,
# but because of escaping issues
# (https://sourceware.org/ml/binutils/2009-05/msg00252.html)
# we embed a dummy path with $ replaced with z and fix it up later.
export LDFLAGS='-Wl,-rpath,zORIGIN/../../../../llvm-build/Release+Asserts/lib'

./configure --enable-gold=default --enable-threads --enable-plugins \
  --prefix=/build/output
make -j8 all
make install

# Save the list of binaries. The sed -i command will leave .orig files behind.
# We don't want them to appear in the for loop below.
bins="$(echo /build/output/*/bin/*)"

# Fix up zORIGIN -> $ORIGIN.
sed -i.orig 's,zORIGIN,$ORIGIN,g' $bins

# Verify that we changed only one byte per executable.
for bin in $bins; do
  test "`cmp -l $bin.orig $bin | wc -l`" = 1 || \
    (echo "$bin: verification failed" && exit 1)
done

rm /build/output/*/bin/*.orig
