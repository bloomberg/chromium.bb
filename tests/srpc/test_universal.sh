#!/bin/bash
#
# Copyright 2008, Google Inc.
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

#
# This test just statically exercises each of the types that can be passed.
# Further testing will fuzz the input values, the string lengths, etc.


set -o nounset
set -o errexit

SEL_LDR=/usr/local/nacl-sdk/nacl/bin/sel_ldr
SEL_UNIVERSAL="/usr/local/nacl-sdk/nacl/bin/sel_universal -p $SEL_LDR"
TMP=/tmp/$(basename $0).output

# Should run for about 15 seconds.
ITERATIONS=100000

RunSelUniversal () {
  ${SEL_UNIVERSAL} -r $ITERATIONS -f $1
}


Banner () {
  echo "# ============================================================"
  echo $*
  echo "# ============================================================"
}

Banner "# Running srpc_test"
RunSelUniversal srpc_test.nexe >$TMP

Banner "# Checking srpc_test"
diff --ignore-space-change  $TMP - <<EOF
EOF


echo "TEST PASSED"
