# Copyright (c) 2012 The Native Client Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# A simple program to run objdump on a file and assert that the ABI TLS
# register appears nowhere in it.  This ensures that the direct register access
# style of TLS is not being used in the IRT blob.

import subprocess
import sys
import re

def Main(args):
  nargs = len(args)
  outfile = None
  if nargs == 4:
    outfile = args[3]
  else:
    assert(nargs == 3)
  arch = args[0]
  objdump = args[1]
  obj_file = args[2]

  whitelist_regex = None
  objdump_args = [objdump, '-d', obj_file]
  if arch == 'x86-32':
    # "%gs:4" is allowed but all other uses of %gs are suspect.
    register = '%gs'
    regex = re.compile(register + r'(?!:0x4\b)')
  elif arch == 'x86-64':
    # Nothing to check.
    regex = None
  elif arch.startswith('arm'):
    # A real reference to r9 should probably be preceded by some character
    # that is not legal for an identifier (e.g., spaces, commas, brackets).
    register = 'r9'
    regex = re.compile('[^_\w]' + register)
    whitelist_regex = re.compile(r'ldr\s+r0,\s*\[r9,\s*#4\]\s*$')
  else:
    print 'Unknown architecture: %s' % arch
    sys.exit(1)

  if regex is not None:
    proc = subprocess.Popen(objdump_args,
                            stdout=subprocess.PIPE,
                            bufsize=-1)
    for line in proc.stdout:
      if regex.search(line) and not (whitelist_regex is not None and
                                     whitelist_regex.search(line)):
        print '%s use found: %s' % (register, line)
        print 'This looks like an %s direct TLS use.' % arch
        print 'Such uses are disallowed by the IRT context constraints.'
        print 'These never happen if -mtls-use-call is used in the compilation.'
        print 'Check that all libraries used in the IRT were compiled that way.'
        sys.exit(1)
    if proc.wait() != 0:
      print 'Command failed: %s' % objdump_args
      sys.exit(1)

  if outfile is not None:
    outf = open(outfile, 'w')
    outf.write('TLS in %s OK\n' % obj_file)
    outf.close()


if __name__ == '__main__':
  Main(sys.argv[1:])
