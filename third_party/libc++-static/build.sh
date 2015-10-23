#!/bin/bash
# Copyright 2015 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

set -eux

REV=245965
DIR=$(mktemp -d -t libcpp)

THIS_DIR="${PWD}/$(dirname "${0}")"

# TODO(thakis): Figure out why our clang complains about visibility and
# redeclarations.
#CXX="$THIS_DIR/../llvm-build/Release+Asserts/bin/clang++"
CXX=c++


FLAGS="-nostdinc++ -O3 -std=c++11 -fstrict-aliasing -fvisibility=hidden -fvisibility-inlines-hidden -mmacosx-version-min=10.6 -arch i386 -arch x86_64 -isysroot $(xcrun -show-sdk-path)"

pushd "${DIR}"

svn co --force https://llvm.org/svn/llvm-project/libcxx/trunk@$REV libcxx
svn co --force https://llvm.org/svn/llvm-project/libcxxabi/trunk@$REV libcxxabi

mkdir libcxxbuild
cd libcxxbuild

mkdir libcxx
pushd libcxx
sed -i '' 's/"default"/"hidden"/g' ../../libcxx/include/__config
"$CXX" -c -I../../libcxx/include/ ../../libcxx/src/*.cpp $FLAGS
popd

mkdir libcxxabi
pushd libcxxabi
sed -i '' 's/"default"/"hidden"/g' ../../libcxxabi/src/*
sed -i '' 's/push(default)/push(hidden)/g' ../../libcxxabi/src/*

# Let the default handler not depend on __cxa_demangle, this saves 0.5MB binary
# size in each binary linking against libc++-static.a
patch -d ../../libcxxabi -p0 < "${THIS_DIR}/libcxxabi.patch"

"$CXX" -c -I../../libcxx/include/ -I../../libcxxabi/include ../../libcxxabi/src/*.cpp $FLAGS
popd

libtool -static -o libc++-static.a libcxx*/*.o

cp libc++-static.a "${THIS_DIR}"
upload_to_google_storage.py -b chromium-libcpp "${THIS_DIR}/libc++-static.a"

popd
rm -rf "${DIR}"
