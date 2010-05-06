#!/bin/bash
# Copyright 2010, Google Inc.
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

#############################################################################
# Runs DejaGnu tests using tha NaCl service runtime and the GCC toolchain.
#
# Supports only x86-64 NaCl.
#############################################################################


function usage ()
{
  cat 1>&2 << END
usage:
  $0 \\
     TOOLCHAIN=<path-to-nacl-toolchain> \\
     WORKDIR=<path-to-gcc-build> \\
     SELLDR=<path-to-sel_ldr>
END
}

# Check arguments and dependencies.
eval "$@"
if [ "x$TOOLCHAIN" = "x" ]; then
  usage
  exit 1
fi
if [ ! -d "$WORKDIR" -o ! -x "$SELLDR" ]; then
  usage
  exit 1
fi
if [ -z "`which autogen`" -o -z "`which runtest`" ]; then
  echo "Error: installation of autogen and dejagnu is required for $0."
  exit 2
fi

# Convert SELLDR and WORKDIR to absolute paths.
if [ "x${SELLDR:0:1}" != "x/" ]; then
  SELLDR="$PWD/$SELLDR"
fi
if [ "x${WORKDIR:0:1}" != "x/" ]; then
  WORKDIR="$PWD/$WORKDIR"
fi

# Determine a new unique directory to store results.
for ((i=1; $i<1000; i++)); do
  RESULTDIR="$WORKDIR/"`printf "results.%.3d" $i`
  if [ ! -e  "$RESULTDIR" ]; then
    break
  fi
done
echo "Results will be copied to: $RESULTDIR"

# Create *.exp files for DejaGnu from file templates.
SITE_EXP="$RESULTDIR/site.exp"
mkdir "$RESULTDIR"

cat >"${SITE_EXP}" \
<<==================================
if ![info exists boards_dir] {
  set boards_dir {}
}

lappend boards_dir "$RESULTDIR"
==================================

cat >"$RESULTDIR/nacl.exp" \
<<==================================
load_generic_config "sim"

set_board_info is_simulator 1
set_board_info sim "$SELLDR"
==================================

# Run the tests and copy logs/summaries regardless of results.
cd "$WORKDIR"
make check \
  DEJAGNU="${SITE_EXP}" \
  LDFLAGS_FOR_TARGET="-lnosys" \
  RUNTESTFLAGS="--target_board=nacl" 2>&1 | \
  tee "$RESULTDIR/testsuite_out.log"
cp -f \
  gcc/testsuite/gcc/gcc.log \
  gcc/testsuite/gcc/gcc.sum \
  gcc/testsuite/g++/g++.log \
  gcc/testsuite/g++/g++.sum \
  nacl64/libstdc++-v3/testsuite/libstdc++.log \
  nacl64/libstdc++-v3/testsuite/libstdc++.sum \
  "$RESULTDIR"

cat "$RESULTDIR/"*.sum | grep -A 7 "Summary ==="
