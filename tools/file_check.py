# Copyright (c) 2012 The Native Client Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""A simple program to run a command and look for a list of expected values.

The list of expected values comes from a file with list of line of
"CHECK: regex" patterns.  This is like LLVM's FileCheck, but gimped.

For now, CHECK: regex'es must be surrounded by either
  (1) surround by C-style comments, or
  (2) begin with '#' comments.

This is to avoid accidentally picking up other uses of the string "CHECK:".
"""

import collections
import re
import subprocess
import sys


def GetAndTestCheckRegexes():
  """Build regexes to look for CHECK: regex.

  Returns a tuple of the possible CHECK patterns.
  Also runs some sanity checks that to see that the regexs do what we expect.
  """
  check_re1 = re.compile(r'/\*\s*CHECK:(.*)\s*\*/')
  check_re2 = re.compile(r'#\s*CHECK:(.*)')
  assert (check_re1.match('/*  CHECK: somereg1 */').group(1).strip() ==
          'somereg1')
  assert (check_re2.match('#  CHECK: somereg2 ').group(1).strip() ==
          'somereg2')
  assert not check_re1.match('/* junk CHECK: somereg1 */')
  assert not check_re2.match('# otherjunk dfCHECK: somereg2 ')
  return check_re1, check_re2


def GetChecklist(src_file):
  """Read in src_file and look for list of CHECKs.

  Return a deque of tuples where the first elem is the CHECK regex literal
  and the second element is the compiled CHECK regex.
  """
  check_re1, check_re2 = GetAndTestCheckRegexes()
  check_list = []
  for line in src_file:
    match = check_re1.search(line)
    if not match:
      match = check_re2.search(line)
    if match:
      match_regex = match.group(1).strip()
      check_tuple = (match_regex, re.compile(match_regex))
      check_list.append(check_tuple)
  if len(check_list) == 0:
    raise Exception('check_list is empty!')
  return collections.deque(check_list)


def Main(args):
  usage = '%prog check_file command-args'
  if len(args) < 2:
    print usage
    return 1

  check_list = None
  with open(args[0]) as check_file:
    check_list = GetChecklist(check_file)
    print 'Num checks: %d' % len(check_list)

  command = args[1:]
  print 'Running command: %s' % ' \\\n\t'.join(command)
  # Check both stdout and stderr.
  proc = subprocess.Popen(command,
                          stdout=subprocess.PIPE,
                          stderr=subprocess.STDOUT,
                          bufsize=-1)
  # stderr should be piped to stdout.
  dump_stdout, dump_stderr = proc.communicate()
  assert not dump_stderr

  for line in dump_stdout.splitlines():
    if len(check_list) == 0:
      print 'PASSED'
      return 0 # Nothing left to check. Success!
    match = check_list[0][1].search(line)
    if match:
      check_list.popleft()

  if check_list:
    print 'Unable to find remaining %d CHECKs:' % len(check_list)
    for check_re in check_list:
      print check_re[0]
    print ''
    print 'Output was:\n%s' % dump_stdout
    print 'FAILED'
    return 1

  print 'PASSED'
  return 0

if __name__ == '__main__':
  sys.exit(Main(sys.argv[1:]))
