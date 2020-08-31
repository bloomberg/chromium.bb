#!/usr/bin/env vpython3
# Copyright 2016 The LUCI Authors. All rights reserved.
# Use of this source code is governed under the Apache License, Version 2.0
# that can be found in the LICENSE file.

import random
import unittest

# Mutates sys.path.
import test_env

from utils import large


class LargeTest(unittest.TestCase):
  def test_1m_1(self):
    array = range(1000000)
    data = large.pack(array)
    self.assertGreater(1000, len(data))
    self.assertEqual(list(array), large.unpack(data))

  def test_1m_1000(self):
    array = [i * 1000 for i in range(1000000)]
    data = large.pack(array)
    self.assertGreater(2000, len(data))
    self.assertEqual(array, large.unpack(data))

  def test_1m_pseudo(self):
    # Compresses a pseudo-random suite. Still compresses very well.
    random.seed(0)
    array = sorted(random.randint(0, 1000000) for _ in range(1000000))
    data = large.pack(array)
    self.assertGreater(302000, len(data))
    self.assertEqual(array, large.unpack(data))

  def test_empty(self):
    self.assertEqual(b'', large.pack([]))
    self.assertEqual([], large.unpack(b''))


if __name__ == '__main__':
  test_env.main()
