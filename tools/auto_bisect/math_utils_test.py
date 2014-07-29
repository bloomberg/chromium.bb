# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import math
import unittest

import math_utils


class MathUtilsTest(unittest.TestCase):
  """Tests for mathematical utility functions."""

  def testTruncatedMeanRaisesError(self):
    """TruncatedMean should raise an error when passed an empty list."""
    with self.assertRaises(TypeError):
      math_utils.TruncatedMean([], 0)

  def testMeanSingleNum(self):
    """Tests the Mean function with a single number."""
    self.assertEqual(3.0, math_utils.Mean([3]))

  def testMeanShortList(self):
    """Tests the Mean function with a short list."""
    self.assertEqual(0.5, math_utils.Mean([-3, 0, 1, 4]))

  def testMeanCompareAlternateImplementation(self):
    """Tests Mean by comparing against an alternate implementation."""
    def AlternateMeanFunction(values):
      """Simple arithmetic mean function."""
      return sum(values) / float(len(values))
    test_values_lists = [[1], [5, 6.5, 1.2, 3], [-3, 0, 1, 4],
                         [-3, -1, 0.12, 0.752, 3.33, 8, 16, 32, 439]]
    for values in test_values_lists:
      self.assertEqual(
          AlternateMeanFunction(values),
          math_utils.Mean(values))

  def testRelativeChange(self):
    """Tests the common cases for calculating relative change."""
    # The change is relative to the first value, regardless of which is bigger.
    self.assertEqual(0.5, math_utils.RelativeChange(1.0, 1.5))
    self.assertEqual(0.5, math_utils.RelativeChange(2.0, 1.0))

  def testRelativeChangeFromZero(self):
    """Tests what happens when relative change from zero is calculated."""
    # If the first number is zero, then the result is not a number.
    self.assertEqual(0, math_utils.RelativeChange(0, 0))
    self.assertTrue(
        math.isnan(math_utils.RelativeChange(0, 1)))
    self.assertTrue(
        math.isnan(math_utils.RelativeChange(0, -1)))

  def testRelativeChangeWithNegatives(self):
    """Tests that relative change given is always positive."""
    self.assertEqual(3.0, math_utils.RelativeChange(-1, 2))
    self.assertEqual(3.0, math_utils.RelativeChange(1, -2))
    self.assertEqual(1.0, math_utils.RelativeChange(-1, -2))


if __name__ == '__main__':
  unittest.main()
