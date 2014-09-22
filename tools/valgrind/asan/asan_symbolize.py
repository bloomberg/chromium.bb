#!/usr/bin/env python

# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from third_party import asan_symbolize

import os
import sys

class LineBuffered(object):
  """Disable buffering on a file object."""
  def __init__(self, stream):
    self.stream = stream

  def write(self, data):
    self.stream.write(data)
    if '\n' in data:
      self.stream.flush()

  def __getattr__(self, attr):
    return getattr(self.stream, attr)


def disable_buffering():
  """Makes this process and child processes stdout unbuffered."""
  if not os.environ.get('PYTHONUNBUFFERED'):
    # Since sys.stdout is a C++ object, it's impossible to do
    # sys.stdout.write = lambda...
    sys.stdout = LineBuffered(sys.stdout)
    os.environ['PYTHONUNBUFFERED'] = 'x'


def set_symbolizer_path():
  """Set the path to the llvm-symbolize binary in the Chromium source tree."""
  if not os.environ.get('LLVM_SYMBOLIZER_PATH'):
    script_dir = os.path.dirname(os.path.abspath(__file__))
    # Assume this script resides three levels below src/ (i.e.
    # src/tools/valgrind/asan/).
    src_root = os.path.join(script_dir, "..", "..", "..")
    symbolizer_path = os.path.join(src_root, 'third_party',
        'llvm-build', 'Release+Asserts', 'bin', 'llvm-symbolizer')
    assert(os.path.isfile(symbolizer_path))
    os.environ['LLVM_SYMBOLIZER_PATH'] = os.path.abspath(symbolizer_path)


def main():
  disable_buffering()
  set_symbolizer_path()
  asan_symbolize.demangle = True
  asan_symbolize.fix_filename_patterns = sys.argv[1:]
  asan_symbolize.logfile = sys.stdin
  loop = asan_symbolize.SymbolizationLoop()
  loop.process_logfile()

if __name__ == '__main__':
  main()
