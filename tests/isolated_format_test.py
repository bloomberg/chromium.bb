#!/usr/bin/env python
# Copyright 2014 The Swarming Authors. All rights reserved.
# Use of this source code is governed under the Apache License, Version 2.0 that
# can be found in the LICENSE file.

import hashlib
import logging
import sys
import unittest

# net_utils adjusts sys.path.
import net_utils

import isolated_format


ALGO = hashlib.sha1


class TestCase(net_utils.TestCase):
  def test_get_hash_algo(self):
    # Tests here assume ALGO is used for default namespaces, check this
    # assumption.
    self.assertIs(isolated_format.get_hash_algo('default'), ALGO)
    self.assertIs(isolated_format.get_hash_algo('default-gzip'), ALGO)


if __name__ == '__main__':
  if '-v' in sys.argv:
    unittest.TestCase.maxDiff = None
  logging.basicConfig(
      level=(logging.DEBUG if '-v' in sys.argv else logging.ERROR))
  unittest.main()
