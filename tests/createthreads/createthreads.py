#!/usr/bin/python
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


import os
import sys
import platform
try:
  import subprocess
except ImportError:
  print "You need python 2.4 or later to run this script"
  sys.exit(-1)

SEL_LDR = "../../scons-out/dbg-linux/obj/service_runtime/sel_ldr"
CREATETHREADS_NEXE = \
  "../../scons-out/nacl/obj/tests/createthreads/createthreads.nexe"

if platform.system() in ("Linux",):
  SEL_LDR = "../../scons-out/dbg-linux/obj/service_runtime/sel_ldr"
elif platform.system() in ("Windows", "Microsoft", "CYGWIN_NT-5.1"):
  SEL_LDR = "../../scons-out/dbg-win/obj/service_runtime/sel_ldr"
elif platform.system() in ("Darwin", "Mac"):
  SEL_LDR = "../../scons-out/dbg-mac/obj/service_runtime/sel_ldr"
else:
  print "Unsupported platform for create threads test!"
  sys.exit(-1)

if __name__ == '__main__':
  sys.exit( subprocess.call([SEL_LDR, "-d", "-f", CREATETHREADS_NEXE]) )
