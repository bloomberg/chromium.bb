# -*- coding: utf-8 -*-
# Copyright 2019 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""The matching utils unit tests."""

from __future__ import print_function

import os
import sys

from chromite.lib import cros_test_lib
from chromite.utils import matching


assert sys.version_info >= (3, 6), 'This module requires Python 3.6+'


class GetMostLikelyMatchedObjectTest(cros_test_lib.TestCase):
  """GetMostLikelyMatchedObject tests."""

  def testNoSimilar(self):
    """Test no results when no similar items."""
    haystack = ['1234', '5678']
    needle = 'abcd'

    result = matching.GetMostLikelyMatchedObject(haystack, needle)
    self.assertFalse(result)

  def testSimilar(self):
    """Test similar items are found."""
    haystack = ['abce', 'aecd', '1234']
    needle = 'abcd'

    result = matching.GetMostLikelyMatchedObject(haystack, needle,
                                                 matched_score_threshold=0.6)
    self.assertCountEqual(['abce', 'aecd'], result)

  def testSimilarThreshold(self):
    """Test the threshold is correctly applied."""
    haystack = ['abce', 'aecd', '1234']
    needle = 'abcd'

    result = matching.GetMostLikelyMatchedObject(haystack, needle,
                                                 matched_score_threshold=0.8)
    self.assertFalse(result)


class FindFilesMatchingTest(cros_test_lib.TempDirTestCase):
  """FindFilesMatching tests."""

  def setUp(self):
    D = cros_test_lib.Directory

    filesystem = (
        D('path', (
            D('to', ('example.txt', 'example.csv', 'thing.txt', 'another.csv')),
            D('excluded', ('example.txt',)),
        )),
    )
    cros_test_lib.CreateOnDiskHierarchy(self.tempdir, filesystem)

    base_path = os.path.join(self.tempdir, 'path', 'to')
    another_csv = os.path.join(base_path, 'another.csv')
    example_csv = os.path.join(base_path, 'example.csv')
    example_txt = os.path.join(base_path, 'example.txt')
    thing_txt = os.path.join(base_path, 'thing.txt')

    self.csvs = [another_csv, example_csv]
    self.txts = [example_txt, thing_txt]

    self.excluded = os.path.join(self.tempdir, 'path', 'excluded')

  def testFindMatching(self):
    """Simple find matching."""
    result = matching.FindFilesMatching('*.csv', target=self.tempdir, cwd='/')
    self.assertCountEqual(self.csvs, result)

  def testExcludeDirs(self):
    """Test the excluded directories works."""
    result = matching.FindFilesMatching('*.txt', target=self.tempdir, cwd='/',
                                        exclude_dirs=(self.excluded,))
    self.assertCountEqual(self.txts, result)

  def testCwd(self):
    """Test the paths change relative to the cwd."""
    result = matching.FindFilesMatching('*.csv',
                                        target='path',
                                        cwd=self.tempdir)
    # Convert the csv paths to be relative to the tempdir.
    expected = [os.path.relpath(x, start=self.tempdir) for x in self.csvs]
    self.assertCountEqual(expected, result)
