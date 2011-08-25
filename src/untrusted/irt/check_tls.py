# Copyright (c) 2011 The Native Client Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# A simple program to run objdump on a file and assert that the ABI TLS
# register appears nowhere in it.  This ensures that the direct register access
# style of TLS is not being used in the IRT blob.

import subprocess
import sys
import re

def Main(args):
  assert(len(args) == 3)
  arch = args[0]
  objdump = args[1]
  obj_file = args[2]
  if arch == 'x86-32':
    objdump_args = [objdump, '-d', obj_file]
    register = '%gs'
    regex = re.compile(register)
  elif arch == 'arm':
    # We need to use -D to disassemble on ARM. See this mail thread:
    # http://lists.gnu.org/archive/html/bug-binutils/2009-06/msg00069.html
    # and bug:
    # http://code.google.com/p/nativeclient/issues/detail?id=1916
    objdump_args = [objdump, '-D', '--section=.text', obj_file]
    # A real reference to r9 should probably be preceded by some character
    # that is not legal for an identifier (e.g., spaces, commas, brackets).
    register = 'r9'
    regex = re.compile('[^_\w]' + register)
  else:
    print 'Unknown architecture: %s' % arch
    sys.exit(1)

  proc = subprocess.Popen(objdump_args,
                          stdout=subprocess.PIPE,
                          bufsize=-1)
  for line in proc.stdout:
    if regex.search(line):
      print '%s use found: %s' % (register, line)
      print 'This looks like an %s direct TLS use.' % arch
      print 'Such uses are disallowed by the IRT context constraints.'
      print 'These never happen if -mtls-use-call is used in the compilation.'
      print 'Check that all libraries used in the IRT were compiled that way.'
      sys.exit(1)


if __name__ == '__main__':
  Main(sys.argv[1:])
