#!/usr/bin/env python

# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from third_party import asan_symbolize

import os
import re
import sys

def fix_filename(file_name):
  for path_to_cut in sys.argv[1:]:
    file_name = re.sub(".*" + path_to_cut, "", file_name)
  file_name = re.sub(".*asan_[a-z_]*.cc:[0-9]*", "_asan_rtl_", file_name)
  file_name = re.sub(".*crtstuff.c:0", "???:0", file_name)
  return file_name

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


def main():
  disable_buffering()
  asan_symbolize.demangle = True
  loop = asan_symbolize.SymbolizationLoop(binary_name_filter=fix_filename)
  loop.process_stdin()

if __name__ == '__main__':
  main()
