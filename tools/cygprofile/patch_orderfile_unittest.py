#!/usr/bin/env vpython
# Copyright 2015 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import unittest

import patch_orderfile
import symbol_extractor


class TestPatchOrderFile(unittest.TestCase):
  def testRemoveSuffixes(self):
    no_clone = 'this.does.not.contain.clone'
    self.assertEquals(no_clone, patch_orderfile.RemoveSuffixes(no_clone))
    with_clone = 'this.does.contain.clone.'
    self.assertEquals(
        'this.does.contain', patch_orderfile.RemoveSuffixes(with_clone))
    with_part = 'this.is.a.part.42'
    self.assertEquals(
        'this.is.a', patch_orderfile.RemoveSuffixes(with_part))

  def testUniqueGenerator(self):
    @patch_orderfile._UniqueGenerator
    def TestIterator():
      yield 1
      yield 2
      yield 1
      yield 3

    self.assertEqual(list(TestIterator()), [1,2,3])


if __name__ == "__main__":
  unittest.main()
