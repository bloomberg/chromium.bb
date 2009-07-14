#!/usr/bin/python
# Copyright 2009, Google Inc.
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

# This script performs the equivalent of the shell command
#
# cp -f >(_inputs) <(INTERMEDIATE_DIR)/nacl_syscall_handlers.$$.c &&
# python ./nacl_syscall_handlers_gen2.py -c -f \
# "Video|Audio|Multimedia" < >(_inputs) >> \
# <(INTERMEDIATE_DIR)/nacl_syscall_handlers.$$.c && mv \
# <(INTERMEDIATE_DIR)/nacl_syscall_hanlders.$$.c \
# nacl_syscall_handlers.c
#
# so that gyp doesn't have to depend on a copy of cygwin.
#
# This script takes all arguments and pass them verbatim to
# nacl_syscall_handlers_gen2.py.  Standard input is read completely
# into memory, written to standard output, buffers flushed, then we
# run nacl_syscall_handlers2.py (with the forementioned arguments)
# with popen and a copy of the input written to it.

import subprocess
import sys

def main(argv):
  usage='Usage: nacl_sycall_handlers_gen3.py [nacl_syscall_handlers_gen2_args]'

  cmd = ['python', 'nacl_syscall_handlers_gen2.py'] + argv[1:]

  data=sys.stdin.read()
  print data
  sys.stdout.flush()

  p = subprocess.Popen(cmd, shell=False, stdin=subprocess.PIPE)
  p.stdin.write(data)
  p.stdin.flush()
  p.stdin.close()
  return p.wait()

if __name__ == '__main__':
  sys.exit(main(sys.argv))
