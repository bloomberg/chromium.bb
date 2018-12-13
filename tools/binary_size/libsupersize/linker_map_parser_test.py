#!/usr/bin/env python
# Copyright 2018 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import glob
import os
import sys
import unittest

import linker_map_parser
import test_util

_SCRIPT_DIR = os.path.dirname(__file__)
_TEST_DATA_DIR = os.path.join(_SCRIPT_DIR, 'testdata', 'linker_map_parser')
_TEST_MAP_PATH = os.path.join(_TEST_DATA_DIR, 'test_lld-lto_v1.map')


def _CompareWithGolden(name=None):

  def real_decorator(func):
    basename = name
    if not basename:
      basename = func.__name__.replace('test_', '')
    golden_path = os.path.join(_TEST_DATA_DIR, basename + '.golden')

    def inner(self):
      actual_lines = func(self)
      test_util.Golden.CheckOrUpdate(golden_path, actual_lines)

    return inner

  return real_decorator


def _ReadMapFile(map_file):
  ret = []
  with open(map_file, 'r') as f:
    for line in f:
      # Strip blank lines and comments.
      stripped_line = line.lstrip()
      if not stripped_line or stripped_line.startswith('#'):
        continue
      ret.append(line)
  return ret


class LinkerMapParserTest(unittest.TestCase):

  @_CompareWithGolden()
  def test_Parser(self):
    ret = []
    map_file = _ReadMapFile(_TEST_MAP_PATH)
    linker_name = linker_map_parser.DetectLinkerNameFromMapFile(iter(map_file))
    section_sizes, raw_symbols = (
        linker_map_parser.MapFileParser().Parse(linker_name, iter(map_file)))
    ret.append('******** section_sizes ********')
    for k, v in sorted(section_sizes.iteritems()):
      ret.append('%-24s %d' % (k, v))
    ret.append('')
    ret.append('******** raw_symbols ********')
    for sym in raw_symbols:
      ret.append(repr(sym))
    return ret


def main():
  argv = sys.argv
  if len(argv) > 1 and argv[1] == '--update':
    argv.pop(0)
    test_util.Golden.EnableUpdate()
    for f in glob.glob(os.path.join(_TEST_DATA_DIR, '*.golden')):
      os.unlink(f)

  unittest.main(argv=argv, verbosity=2)


if __name__ == '__main__':
  main()
