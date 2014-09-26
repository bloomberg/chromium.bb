# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import unittest

import bisect_results
import ttest


class ConfidenceScoreTest(unittest.TestCase):

  def testConfidenceScoreIsZeroOnTooFewLists(self):
    self.assertEqual(bisect_results.ConfidenceScore([], [[1], [2]]), 0.0)
    self.assertEqual(bisect_results.ConfidenceScore([[1], [2]], []), 0.0)
    self.assertEqual(bisect_results.ConfidenceScore([[1]], [[1], [2]]), 0.0)
    self.assertEqual(bisect_results.ConfidenceScore([[1], [2]], [[1]]), 0.0)

  def testConfidenceScoreIsZeroOnEmptyLists(self):
    self.assertEqual(bisect_results.ConfidenceScore([[], []], [[1], [2]]), 0.0)
    self.assertEqual(bisect_results.ConfidenceScore([[1], [2]], [[], []]), 0.0)

  def testConfidenceScoreIsUsingTTestWelchsTTest(self):
    original_WelchsTTest = ttest.WelchsTTest
    try:
      ttest.WelchsTTest = lambda _sample1, _sample2: (0, 0, 0.42)
      self.assertAlmostEqual(
        bisect_results.ConfidenceScore([[1], [1]], [[2], [2]]), 58.0)
    finally:
      ttest.WelchsTTest = original_WelchsTTest


class BisectResulstsTest(unittest.TestCase):
  # TODO(sergiyb): Write tests for GetResultDicts when it is broken into smaller
  # pieces.
  pass


if __name__ == '__main__':
  unittest.main()
