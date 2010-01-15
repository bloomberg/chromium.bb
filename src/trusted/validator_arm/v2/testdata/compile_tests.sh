#!/bin/bash
# Copyright 2009 The Native Client Authors.  All rights reserved.
# Use of this source code is governed by a BSD-style license that can
# be found in the LICENSE file.
# Copyright 2009 Google Inc.

# NOTE: You must have some arm crosscompiler available to run this
#       here we (ab)use the trusted CC compiler provided by
#       "source tools/llvm/setup_arm_trusted_toolchain.sh"

readonly ARM_CROSS_COMPILER="${ARM_CC}"
for t in *.S ; do
  echo "compiling $t -> ${t%.*}.nexe"
  ${ARM_CROSS_COMPILER} -static -nodefaultlibs -nostdlib -o ${t%.*}.nexe $t
done
