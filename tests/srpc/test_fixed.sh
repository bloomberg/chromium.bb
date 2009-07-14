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

Banner "# Running srpc_test"
RunSelUniversal srpc_test.nexe <<EOF  >$TMP
service
rpc bool b(0) * b(0)
rpc double d(3.1415926) * d(0)
rpc int i(-1216) * i(0)
rpc string s("a tale, told by an idiot") * i(0)
rpc char_array C(9,abcdefghi) * C(9)
rpc double_array D(5,1,.5,.25,.125,.0625) * D(5)
rpc int_array I(6,1,1,2,3,5,8) * I(6)
EOF

Banner "# Checking srpc_test"
diff --ignore-space-change  $TMP - <<EOF
RPC Name                 Input args Output args
  0 service_discovery               C
  1 bool                 b          b
  2 double               d          d
  3 int                  i          i
  4 string               s          i
  5 char_array           C          C
  6 double_array         D          D
  7 int_array            I          I
bool RESULTS:   b(1)
double RESULTS:   d(-3.141593)
int RESULTS:   i(1216)
string RESULTS:   i(24)
char_array RESULTS:   ihgfedcba
double_array RESULTS:   D(5,0.062500,0.125000,0.250000,0.500000,1.000000)
int_array RESULTS:   I(6,8,5,3,2,1,1)
EOF


echo "TEST PASSED"
