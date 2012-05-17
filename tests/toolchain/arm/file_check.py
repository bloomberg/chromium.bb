# Copyright (c) 2012 The Native Client Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# A simple program to run objdump on a file and assert that the expected
# code is generated.  Like LLVM's FileCheck.
#
# usage: %prog <objdump_command> <obj_file> <src_file_with_checks>

import collections
import re
import subprocess
import sys

def GetChecklist(src_file):
  check_re = re.compile('/\* CHECK: (.*) \*/')
  check_list = []
  for line in src_file.readlines():
    match = check_re.search(line)
    if match:
      check_list.append((match.group(1), re.compile(match.group(1))))
  return collections.deque(check_list)

def Main(args):
  assert(len(args) == 3)
  objdump = args[0]
  obj_file = args[1]
  src_file = open(args[2], 'r')

  # Scan the source file for the "CHECK" list.
  check_list = GetChecklist(src_file)
  src_file.close()

  objdump_args = [objdump, '-d', obj_file]
  proc = subprocess.Popen(objdump_args,
                          stdout=subprocess.PIPE,
                          bufsize=-1)
  dump_stdout, dump_stderr = proc.communicate()

  for line in dump_stdout.split('\n'):
    if len(check_list) == 0:
      print 'PASSED'
      return 0 # Nothing left to check. Success!
    match = check_list[0][1].search(line)
    if match:
      check_list.popleft()

  if check_list != []:
    print 'Unable to find remaining CHECKs from objdump:'
    for check_re in check_list:
      print check_re[0]
    print 'Objdump was:\n%s' % dump_stdout
    print 'FAILED'
    sys.exit(1)

  print 'PASSED'
  return 0

if __name__ == '__main__':
  Main(sys.argv[1:])
