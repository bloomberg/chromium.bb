#!/usr/bin/env python
# Copyright (c) 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import logging
import os
import subprocess
import sys
import unittest

BASE_DIR = os.path.dirname(os.path.abspath(__file__))


class TestSwarm(unittest.TestCase):
  def test_example(self):
    # pylint: disable=W0101
    # A user should be able to trigger a swarm job and return results.
    cmd = [
      sys.executable,
      os.path.normpath(
          os.path.join(BASE_DIR, '..', 'example', 'run_example_swarm.py')),
    ]
    p = subprocess.Popen(
        cmd,
        stdin=subprocess.PIPE,
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT)
    out = p.communicate()[0]
    logging.debug(out)
    self.assertEqual(0, p.returncode, out)


if __name__ == '__main__':
  logging.basicConfig(
      level=logging.DEBUG if '-v' in sys.argv else logging.ERROR)
  if '-v' in sys.argv:
    unittest.TestCase.maxDiff = None
  unittest.main()
