# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Measurement smoke test to make sure that no new action_name_to_run is
defined."""

import os
import optparse
import logging
import unittest

from telemetry import benchmark as benchmark_module
from telemetry.core import discover
from telemetry.page import page_test
from telemetry.unittest_util import options_for_unittests
from telemetry.util import classes
from telemetry.web_perf import timeline_based_measurement


# Do NOT add new items to this list!
# crbug.com/418375
_ACTION_NAMES_WHITE_LIST = (
  'RunPageInteractions',
)


def _GetAllPossiblePageTestInstances():
  page_test_instances = []
  measurements_dir = os.path.dirname(__file__)
  top_level_dir = os.path.dirname(measurements_dir)
  benchmarks_dir = os.path.join(top_level_dir, 'benchmarks')

  # Get all page test instances from measurement classes that are directly
  # constructable
  all_measurement_classes = discover.DiscoverClasses(
      measurements_dir, top_level_dir, page_test.PageTest).values()
  for measurement_class in all_measurement_classes:
    if classes.IsDirectlyConstructable(measurement_class):
      page_test_instances.append(measurement_class())

  all_benchmarks_classes = discover.DiscoverClasses(
      benchmarks_dir, top_level_dir, benchmark_module.Benchmark).values()

  # Get all page test instances from defined benchmarks.
  # Note: since this depends on the command line options, there is no guaranteed
  # that this will generate all possible page test instances but it's worth
  # enough for smoke test purpose.
  for benchmark_class in all_benchmarks_classes:
    options = options_for_unittests.GetCopy()
    parser = optparse.OptionParser()
    benchmark_class.AddCommandLineArgs(parser)
    benchmark_module.AddCommandLineArgs(parser)
    benchmark_class.SetArgumentDefaults(parser)
    options.MergeDefaultValues(parser.get_default_values())
    pt = benchmark_class().CreatePageTest(options)
    if not isinstance(pt, timeline_based_measurement.TimelineBasedMeasurement):
      page_test_instances.append(pt)

  return page_test_instances


class MeasurementSmokeTest(unittest.TestCase):
  # TODO(nednguyen): Remove this test when crbug.com/418375 is marked fixed.
  def testNoNewActionNameToRunUsed(self):
    invalid_tests = []
    for test in _GetAllPossiblePageTestInstances():
      if not hasattr(test, 'action_name_to_run'):
        invalid_tests.append(test)
        logging.error('Test %s missing action_name_to_run attribute.',
                      test.__class__.__name__)
      if test.action_name_to_run not in _ACTION_NAMES_WHITE_LIST:
        invalid_tests.append(test)
        logging.error('Page test %s has invalid action_name_to_run: %s' %
                     (test.__class__.__name__, repr(test.action_name_to_run)))
    self.assertFalse(
      invalid_tests,
      'New page tests with invalid action_name_to_run found. Please only use '
      'action_name_to_run="RunPageInteractions" (crbug.com/418375).')
