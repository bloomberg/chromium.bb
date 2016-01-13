# Copyright 2015 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import unittest

from infra_libs.ts_mon.common import distribution


class BucketerTestBase(unittest.TestCase):
  def assertBucketCounts(self, b, expected_total):
    self.assertEquals(expected_total - 2, b.num_finite_buckets)
    self.assertEquals(expected_total, b.total_buckets)
    self.assertEquals(0, b.underflow_bucket)
    self.assertEquals(expected_total - 1, b.overflow_bucket)

  def assertBoundaries(self, b, expected_finite_upper_boundaries):
    self.assertEquals(
        b.num_finite_buckets, len(expected_finite_upper_boundaries))

    expected_boundaries = [(float('-Inf'), 0)]

    previous = 0
    for value in expected_finite_upper_boundaries:
      expected_boundaries.append((previous, value))
      previous = value

    expected_boundaries.append((previous, float('Inf')))

    self.assertEquals(expected_boundaries, list(b.all_bucket_boundaries()))
    for i, expected in enumerate(expected_boundaries):
      self.assertEquals(expected, b.bucket_boundaries(i))
      self.assertEquals(i, b.bucket_for_value(expected[0]))
      self.assertEquals(i, b.bucket_for_value(expected[0] + 0.5))
      self.assertEquals(i, b.bucket_for_value(expected[1] - 0.5))

    with self.assertRaises(IndexError):
      b.bucket_boundaries(-1)
    with self.assertRaises(IndexError):
      b.bucket_boundaries(len(expected_boundaries))


class FixedWidthBucketerTest(BucketerTestBase):
  def test_negative_size(self):
    with self.assertRaises(ValueError):
      distribution.FixedWidthBucketer(width=10, num_finite_buckets=-1)

  def test_negative_width(self):
    with self.assertRaises(ValueError):
      distribution.FixedWidthBucketer(width=-1, num_finite_buckets=1)

  def test_zero_size(self):
    b = distribution.FixedWidthBucketer(width=10, num_finite_buckets=0)

    self.assertBucketCounts(b, 2)
    self.assertBoundaries(b, [])

  def test_one_size(self):
    b = distribution.FixedWidthBucketer(width=10, num_finite_buckets=1)

    self.assertBucketCounts(b, 3)
    self.assertBoundaries(b, [10])


class GeometricBucketerTest(BucketerTestBase):
  def test_negative_size(self):
    with self.assertRaises(ValueError):
      distribution.GeometricBucketer(num_finite_buckets=-1)

  def test_small_scale(self):
    with self.assertRaises(ValueError):
      distribution.GeometricBucketer(growth_factor=-1)
    with self.assertRaises(ValueError):
      distribution.GeometricBucketer(growth_factor=0)
    with self.assertRaises(ValueError):
      distribution.GeometricBucketer(growth_factor=1)

  def test_zero_size(self):
    b = distribution.GeometricBucketer(num_finite_buckets=0)

    self.assertBucketCounts(b, 2)
    self.assertBoundaries(b, [])

  def test_large_size(self):
    b = distribution.GeometricBucketer(growth_factor=4, num_finite_buckets=4)

    self.assertBucketCounts(b, 6)
    self.assertBoundaries(b, [1, 4, 16, 64])


class CustomBucketerTest(BucketerTestBase):
  def test_boundaries(self):
    b = distribution.Bucketer(width=10, growth_factor=2, num_finite_buckets=4)

    self.assertBucketCounts(b, 6)
    self.assertBoundaries(b, [11, 22, 34, 48])


class DistributionTest(unittest.TestCase):
  def test_add(self):
    d = distribution.Distribution(distribution.GeometricBucketer())
    self.assertEqual(0, d.sum)
    self.assertEqual(0, d.count)
    self.assertEqual({}, d.buckets)

    d.add(1)
    d.add(10)
    d.add(100)

    self.assertEqual(111, d.sum)
    self.assertEqual(3, d.count)
    self.assertEqual({2: 1, 6: 1, 11: 1}, d.buckets)

    d.add(50)

    self.assertEqual(161, d.sum)
    self.assertEqual(4, d.count)
    self.assertEqual({2: 1, 6: 1, 10: 1, 11: 1}, d.buckets)

  def test_add_on_bucket_boundary(self):
    d = distribution.Distribution(distribution.FixedWidthBucketer(width=10))

    d.add(10)

    self.assertEqual(10, d.sum)
    self.assertEqual(1, d.count)
    self.assertEqual({2: 1}, d.buckets)

    d.add(0)

    self.assertEqual(10, d.sum)
    self.assertEqual(2, d.count)
    self.assertEqual({1: 1, 2: 1}, d.buckets)

  def test_underflow_bucket(self):
    d = distribution.Distribution(distribution.FixedWidthBucketer(width=10))

    d.add(-1)

    self.assertEqual(-1, d.sum)
    self.assertEqual(1, d.count)
    self.assertEqual({0: 1}, d.buckets)

    d.add(-1000000)

    self.assertEqual(-1000001, d.sum)
    self.assertEqual(2, d.count)
    self.assertEqual({0: 2}, d.buckets)

  def test_overflow_bucket(self):
    d = distribution.Distribution(
        distribution.FixedWidthBucketer(width=10, num_finite_buckets=10))

    d.add(100)

    self.assertEqual(100, d.sum)
    self.assertEqual(1, d.count)
    self.assertEqual({11: 1}, d.buckets)

    d.add(1000000)

    self.assertEqual(1000100, d.sum)
    self.assertEqual(2, d.count)
    self.assertEqual({11: 2}, d.buckets)
