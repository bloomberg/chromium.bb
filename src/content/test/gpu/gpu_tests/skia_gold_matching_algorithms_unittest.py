# Copyright 2020 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import unittest

from gpu_tests import skia_gold_matching_algorithms as algo


class ExactMatchingAlgorithmTest(unittest.TestCase):
  def testGetCmdline(self):
    a = algo.ExactMatchingAlgorithm()
    self.assertEqual(a.GetCmdline(), [])


class FuzzyMatchingAlgorithmTest(unittest.TestCase):
  def testGetCmdline(self):
    a = algo.FuzzyMatchingAlgorithm(1, 2)
    cmdline = a.GetCmdline()
    self.assertEqual(cmdline, [
        '--add-test-optional-key',
        'image_matching_algorithm:fuzzy',
        '--add-test-optional-key',
        'fuzzy_max_different_pixels:1',
        '--add-test-optional-key',
        'fuzzy_pixel_delta_threshold:2',
    ])

  def testInvalidArgs(self):
    with self.assertRaises(AssertionError):
      algo.FuzzyMatchingAlgorithm(-1, 0)
    with self.assertRaises(AssertionError):
      algo.FuzzyMatchingAlgorithm(0, -1)


class SobelMatchingAlgorithmTest(unittest.TestCase):
  def testGetCmdline(self):
    a = algo.SobelMatchingAlgorithm(1, 2, 3)
    cmdline = a.GetCmdline()
    self.assertEqual(cmdline, [
        '--add-test-optional-key',
        'image_matching_algorithm:sobel',
        '--add-test-optional-key',
        'fuzzy_max_different_pixels:1',
        '--add-test-optional-key',
        'fuzzy_pixel_delta_threshold:2',
        '--add-test-optional-key',
        'sobel_edge_threshold:3',
    ])

  def testInvalidArgs(self):
    with self.assertRaises(AssertionError):
      algo.SobelMatchingAlgorithm(1, 2, -1)
    with self.assertRaises(AssertionError):
      algo.SobelMatchingAlgorithm(1, 2, 256)
    with self.assertRaises(RuntimeError):
      algo.SobelMatchingAlgorithm(1, 2, 255)
