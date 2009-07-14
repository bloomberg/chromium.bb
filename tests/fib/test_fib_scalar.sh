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


set -o nounset
set -o errexit

SEL_LDR=../../service_runtime/sel_ldr
SEL_UNIVERSAL="../../service_runtime/sel_universal -p $SEL_LDR"
TMP=/tmp/$(basename $0).output

RunSelUniversal () {
  ${SEL_UNIVERSAL} -f $1
}


Banner () {
  echo "# ============================================================"
  echo $*
  echo "# ============================================================"
}

Banner "# Running fib_array"
RunSelUniversal fib_scalar.nexe <<EOF  >$TMP
service
rpc fib i(1) i(1) i(5) * i(0)
rpc fib i(2) i(3) i(10) * i(0)
rpc fib i(1) i(1) i(15) * i(0)
EOF

Banner "# Checking fib_array"
diff --ignore-space-change  $TMP - <<EOF
RPC Name                 Input args Output args
  0 service_discovery               C
  1 fib                  iii        i
fib RESULTS:   i(8)
fib RESULTS:   i(233)
fib RESULTS:   i(987)
EOF


# TODO(sehr): Change to report result correctly for the automated tests.
echo "TEST PASSED"
