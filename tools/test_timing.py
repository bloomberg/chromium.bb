#!/usr/bin/python2.4
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

"""Program to parse hammer output and sort tests by how long they took to run.

The output should be from the CommandTest in SConstruct.
"""

import getopt
import re
import sys


class Analyzer(object):
  """Basic run time analysis (sorting, so far)."""

  def __init__(self):
    self._data = []  # list of (time, test-name) tuples
  # enddef

  def AddData(self, execution_time, test_name, mode):
    self._data.append((execution_time, test_name, mode))
  # enddef

  def Sort(self):
    self._data.sort(None, lambda x: x[0], True)
  # enddef

  def Top(self, n):
    return self._data[:n]
  # enddef
# endclass


def TrimTestName(name):
  s = '/scons-out/'
  ix = name.find(s)
  if ix < 0:
    return name[ix + len(s):]
  # endif
  return name
# enddef


def Usage():
  print >>sys.stderr, 'Usage: test_timing [-n top-n-to-print]'
# enddef


def main(argv):
  top_n = 10

  try:
    optlist, argv = getopt.getopt(argv[1:], 'n:')
  except getopt.error, e:
    print >>sys.stderr, str(e)
    Usage()
    return 1
  # endtry

  for opt, val in optlist:
    if opt == '-n':
      try:
        top_n = int(val)
      except ValueError:
        print >>sys.stderr, 'test_timing: -n arg should be an integer'
        Usage()
        return 1
      # endtry
    # endif
  # endfor

  mode = 'Unknown'
  mode_nfa = re.compile(r'^running.*scons-out/((opt|dbg)-linux)')
  nfa = re.compile(r'^Test (.*) took ([.0-9]*) secs')
  analyzer = Analyzer()

  for line in sys.stdin:
    mobj = mode_nfa.match(line)
    if mobj is not None:
      mode = mobj.group(1)
      continue
    # endif
    mobj = nfa.match(line)
    if mobj is not None:
      analyzer.AddData(float(mobj.group(2)), mobj.group(1), mode)
    # endif
  # endfor
  analyzer.Sort()

  print '%-12s %-9s %s' % ('Time', 'Mode', 'Test Name')
  print '%-12s %-9s %s' % (12*'-', 9*'-', '---------')
  for time, name, mode in analyzer.Top(top_n):
    print '%12.8f %-9s %s' % (time, mode, TrimTestName(name))
  # endfor
  return 0
# enddef


if __name__ == '__main__':
  sys.exit(main(sys.argv))
# endif
