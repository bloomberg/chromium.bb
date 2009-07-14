#!/usr/bin/python
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


"""Tools for exporting NativeClient ABI header files.

This module is used to export NativeClient ABI header files -- which are in
the native_client/src/trusted/service_runtime/include directory -- for use for NativeClient
applications compilation.
"""

import os
import re
import sys

def ProcessStreams(instr, outstr):
  """Read internal version of header file from instr, write exported version
  to outstr.
  """

  pat = r'\b(?:nacl_abi_|NaClAbi|NACL_ABI_)([A-Za-z0-9_]*)'
  cpat = re.compile(pat)

  inc = r'^#\s*include\s+"native_client(?:/src/trusted/service_runtime)?/include/([^"]*)"'
  incpat = re.compile(inc)

  for str in instr:
    if incpat.search(str):
      print >>outstr, incpat.sub(r'#include <\1>', str)
    else:
      print >>outstr, cpat.sub(r'\1', str),
    # endif
  # endfor
# enddef

def ProcessFileList(flist):
  for f in flist:
    ProcessStreams(open(f), sys.stdout);
  # endfor
# enddef

def ProcessDir(srcdir, dstdir):
  if not os.path.isdir(srcdir):
    return
  # endif
  if not os.path.isdir(dstdir):
    os.makedirs(dstdir)
  # endif
  for fn in os.listdir(srcdir):
    srcpath = os.path.join(srcdir, fn)
    dstpath = os.path.join(dstdir, fn)
    if os.path.isfile(srcpath) and fn.endswith('.h'):
      ProcessStreams(open(srcpath),
                     open(dstpath, 'w'));
    elif os.path.isdir(srcpath):
      ProcessDir(srcpath, dstpath)
    # endif
  # endfor
# enddef

def main(argv):
  if len(argv) != 3:
    print >>sys.stderr, ('Usage: ./export_header source/include/path'
                         ' dest/include/path')
    return 1
  # endif
  ProcessDir(argv[1], argv[2]);
  return 0
# enddef

if __name__ == '__main__':
  sys.exit(main(sys.argv))
# endif
