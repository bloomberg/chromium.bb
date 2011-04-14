# Copyright (c) 2011 The Native Client Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# A simple program to run objdump on a file and assert that '%gs'
# appears nowhere in it.  This ensures that the direct register access
# style of TLS is not being used in the IRT blob.

import subprocess
import sys


def Main(args):
  objdump = args[0]
  obj_file = args[1]
  assert(len(args) == 2)
  proc = subprocess.Popen([objdump, '-d', obj_file],
                          stdout=subprocess.PIPE,
                          bufsize=-1)
  for line in proc.stdout:
    if '%gs' in line:
      print '%%gs use found: %s' % line
      print 'This looks like an x86-32 direct TLS use.'
      print 'Such uses are disallowed by the IRT execution context constraints.'
      print 'These never happen if -mtls-use-call is used in the compilation.'
      print 'Check that all libraries used in the IRT were compiled that way.'
      sys.exit(1)


if __name__ == '__main__':
  Main(sys.argv[1:])
