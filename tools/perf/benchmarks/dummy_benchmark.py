# Copyright 2015 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Dummy benchmarks for the bisect FYI integration tests.

The number they produce aren't meant to represent any actual performance
data of the browser. For more information about these dummy benchmarks,
see: https://goo.gl/WvZiiW
"""

import random

from core import perf_benchmark

from telemetry import benchmark
from telemetry.value import scalar
from telemetry.page import legacy_page_test
from telemetry.web_perf import timeline_based_measurement

from page_sets import dummy_story_set

class _DummyTest(legacy_page_test.LegacyPageTest):

  def __init__(self, avg, std):
    super(_DummyTest, self).__init__()
    self._avg = avg
    self._std = std

  def ValidateAndMeasurePage(self, page, tab, results):
    del tab  # unused
    results.AddValue(scalar.ScalarValue(
        page=page,
        name='gaussian-value', units='ms',
        value=random.gauss(self._avg, self._std),
        description=('Random number that follows the Gaussian distribution '
                     'with mean=%s and std=%s' % (self._avg, self._std))))


class _DummyBenchmark(perf_benchmark.PerfBenchmark):
  page_set = dummy_story_set.DummyStorySet


@benchmark.Info(emails=['nednguyen@google.com'], component='Speed>Telemetry')
class DummyBenchmarkOne(_DummyBenchmark):
  """A low noise benchmark with mean=100 & std=1."""

  def CreatePageTest(self, options):
    return _DummyTest(168, 1)

  @classmethod
  def Name(cls):
    return 'dummy_benchmark.stable_benchmark_1'


@benchmark.Info(emails=['nednguyen@google.com'], component='Speed>Telemetry')
class DummyBenchmarkTwo(_DummyBenchmark):
  """A noisy benchmark with mean=50 & std=20."""

  def CreatePageTest(self, options):
    return _DummyTest(50, 20)

  @classmethod
  def Name(cls):
    return 'dummy_benchmark.noisy_benchmark_1'


@benchmark.Info(emails=['eakuefner@chromium.org', 'simonhatch@chromium.org'])
class DummyBenchmarkThree(perf_benchmark.PerfBenchmark):
  """A test benchmark for outputting histograms."""
  page_set = dummy_story_set.DummyStorySet

  def CreateCoreTimelineBasedMeasurementOptions(self):
    options = timeline_based_measurement.Options(
        timeline_based_measurement.DEBUG_OVERHEAD_LEVEL)
    options.SetTimelineBasedMetrics(['sampleMetric'])
    return options

  @classmethod
  def Name(cls):
    return 'dummy_benchmark.histogram_benchmark_1'
