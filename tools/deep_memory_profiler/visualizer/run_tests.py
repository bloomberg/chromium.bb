#!/usr/bin/python
# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# This file is expected to be used under another directory to use,
# so we disable checking import path of GAE tools from this directory.
# pylint: disable=F0401

import os
import sys
import unittest


def main():
  if len(sys.argv) != 2:
    sys.stderr.write("""Usage: run_tests.py <path/to/google_appengine>""")
    return 1

  sys.path.insert(0, sys.argv.pop(1))
  import dev_appserver
  dev_appserver.fix_sys_path()
  path = os.path.dirname(os.path.abspath(__file__))
  suite = unittest.loader.TestLoader().discover(
    path,
    pattern='*_unittest.py'
  )
  unittest.TextTestRunner(verbosity=2).run(suite)


if __name__ == '__main__':
  sys.exit(main())
