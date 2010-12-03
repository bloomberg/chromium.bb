#!/bin/bash
#
# Copyright (c) 2010 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
#
# A script that runs a clang plugin against the chrome target on Linux.

export CXXFLAGS="-Xclang -load  -Xclang /work/llvm/Release+Asserts/lib/libFindBadConstructs.so -Xclang -plugin -Xclang find-bad-constructs"
export CXX=`dirname $0`/cxx_wrapper.sh
export LINK=`dirname $0`/link_wrapper.sh

make -j2 chrome
