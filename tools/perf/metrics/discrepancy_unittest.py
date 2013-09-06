# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
import unittest
import random

from metrics import discrepancy

def Relax(samples, iterations=10):
  ''' Lloyd relaxation in 1D. Keeps the position of the first and last
      sample.
  '''
  for _ in xrange(0, iterations):
    voronoi_boundaries = []
    for i in xrange(1, len(samples)):
      voronoi_boundaries.append((samples[i] + samples[i-1]) * 0.5)

    relaxed_samples = []
    relaxed_samples.append(samples[0])
    for i in xrange(1, len(samples)-1):
      relaxed_samples.append(
          (voronoi_boundaries[i-1] + voronoi_boundaries[i]) * 0.5)
    relaxed_samples.append(samples[-1])
    samples = relaxed_samples
  return samples

class DiscrepancyUnitTest(unittest.TestCase):
  def testRandom(self):
    ''' Generates 10 sets of 10 random samples, computes the discrepancy,
        relaxes the samples using Llloyd's algorithm in 1D, and computes the
        discrepancy of the relaxed samples. Discrepancy of the relaxed samples
        must be less than or equal to the discrepancy of the original samples.
    '''
    random.seed(1234567)
    for _ in xrange(0, 10):
      samples = []
      num_samples = 10
      clock = 0.0
      samples.append(clock)
      for _ in xrange(1, num_samples):
        clock += random.random()
        samples.append(clock)
      samples = discrepancy.NormalizeSamples(samples)[0]
      d = discrepancy.Discrepancy(samples)

      relaxed_samples = Relax(samples)
      d_relaxed = discrepancy.Discrepancy(relaxed_samples)

      self.assertLessEqual(d_relaxed, d)

  def testAnalytic(self):
    ''' Computes discrepancy for sample sets with known discrepancy. '''
    interval_multiplier = 100000

    samples = [1.0/8.0, 3.0/8.0, 5.0/8.0, 7.0/8.0]
    d = discrepancy.Discrepancy(samples, interval_multiplier)
    self.assertAlmostEquals(round(d, 2), 0.25)

    samples = [0.0, 1.0/3.0, 2.0/3.0, 1.0]
    d = discrepancy.Discrepancy(samples, interval_multiplier)
    self.assertAlmostEquals(round(d, 2), 0.5)

    samples = discrepancy.NormalizeSamples(samples)[0]
    d = discrepancy.Discrepancy(samples, interval_multiplier)
    self.assertAlmostEquals(round(d, 2), 0.25)

    time_stamps_a = [0, 1, 2, 3, 5, 6]
    time_stamps_b = [0, 1, 2, 3, 5, 7]
    time_stamps_c = [0, 2, 3, 4]
    time_stamps_d = [0, 2, 3, 4, 5]
    d_abs_a = discrepancy.FrameDiscrepancy(time_stamps_a, True,
                                           interval_multiplier)
    d_abs_b = discrepancy.FrameDiscrepancy(time_stamps_b, True,
                                           interval_multiplier)
    d_abs_c = discrepancy.FrameDiscrepancy(time_stamps_c, True,
                                           interval_multiplier)
    d_abs_d = discrepancy.FrameDiscrepancy(time_stamps_d, True,
                                           interval_multiplier)
    d_rel_a = discrepancy.FrameDiscrepancy(time_stamps_a, False,
                                           interval_multiplier)
    d_rel_b = discrepancy.FrameDiscrepancy(time_stamps_b, False,
                                           interval_multiplier)
    d_rel_c = discrepancy.FrameDiscrepancy(time_stamps_c, False,
                                           interval_multiplier)
    d_rel_d = discrepancy.FrameDiscrepancy(time_stamps_d, False,
                                           interval_multiplier)

    self.assertLess(d_abs_a, d_abs_b)
    self.assertLess(d_rel_a, d_rel_b)
    self.assertLess(d_rel_d, d_rel_c)
    self.assertEquals(round(d_abs_d, 2), round(d_abs_c, 2))
