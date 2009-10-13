#!/usr/bin/python
#
# Copyright 2008, 2009, The Native Client Authors.  All rights reserved.
# Use of this source code is governed by a BSD-style license that can
# be found in the LICENSE file.


"""Tools for exporting Native Client ABI header files.

This module is used to export Native Client ABI header files -- which
are in the native_client/src/trusted/service_runtime/include directory
-- for use with the SDK and newlib to compile NaCl applications.
"""

import os
import re
import sys


UNMODIFIED_DIR=['nacl','abi']


def ProcessStream(instr, outstr):
  """Read internal version of header file from instr, write exported
  version to outstr.  The two transformations are to remove nacl_abi_
  prefixes (in its various incarnations), and to change include
  directives from the Google coding style
  "native_client/include/foo/bar.h" to <foo/bar.h>, and from
  "native_client/src/trusted/service_runtime/include/baz/quux.h" to
  <baz/quux.h>."""

  pat = r'\b(?:nacl_abi_|NaClAbi|NACL_ABI_)([A-Za-z0-9_]*)'
  cpat = re.compile(pat)

  inc = (r'^#\s*include\s+"native_client(?:/src/trusted/service_runtime)?'+
         r'/include/([^"]*)"')
  cinc = re.compile(inc)

  for line in instr:
    if cinc.search(line):
      print >>outstr, cinc.sub(r'#include <\1>', line)
    else:
      print >>outstr, cpat.sub(r'\1', line),
    # endif
  # endfor
# enddef


def CopyStream(instr, outstr):
  for line in instr:
    outstr.write(line)
  # endfor
# enddef


def ProcessDir(srcdir, dstdir, unmodified_dstdir):
  if not os.path.isdir(srcdir):
    return
  # endif
  if not os.path.isdir(dstdir):
    os.makedirs(dstdir)
  # endif
  if not os.path.isdir(unmodified_dstdir):
    os.makedirs(unmodified_dstdir)
  # endif
  for fn in os.listdir(srcdir):
    srcpath = os.path.join(srcdir, fn)
    dstpath = os.path.join(dstdir, fn)
    undstpath = os.path.join(unmodified_dstdir, fn)
    if os.path.isfile(srcpath) and fn.endswith('.h'):
      ProcessStream(open(srcpath),
                    open(dstpath, 'w'))
      CopyStream(open(srcpath),
                 open(undstpath, 'w'))
    elif os.path.isdir(srcpath):
      ProcessDir(srcpath, dstpath, undstpath)
    # endif
  # endfor
# enddef

def main(argv):
  if len(argv) != 3:
    print >>sys.stderr, ('Usage: ./export_header source/include/path'
                         ' dest/include/path')
    return 1
  # endif
  ProcessDir(argv[1], argv[2],
             reduce(os.path.join, UNMODIFIED_DIR, argv[2]));
  return 0
# enddef

if __name__ == '__main__':
  sys.exit(main(sys.argv))
# endif
