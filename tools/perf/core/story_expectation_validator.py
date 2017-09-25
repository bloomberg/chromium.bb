#!/usr/bin/env python
# Copyright 2017 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Script to check validity of StoryExpectations."""
import os

from core import benchmark_finders
from core import path_util
path_util.AddTelemetryToPath()
path_util.AddAndroidPylibToPath()


from telemetry import decorators
from telemetry.internal.browser import browser_options


CLUSTER_TELEMETRY_DIR = os.path.join(
    path_util.GetChromiumSrcDir(), 'tools', 'perf', 'contrib',
    'cluster_telemetry')
CLUSTER_TELEMETRY_BENCHMARKS = [
    ct_benchmark.Name() for ct_benchmark in
    benchmark_finders.GetBenchmarksInSubDirectory(CLUSTER_TELEMETRY_DIR)
]


# TODO(rnephew): Remove this check when it is the norm to not use decorators.
def check_decorators(benchmarks):
  for benchmark in benchmarks:
    if (decorators.GetDisabledAttributes(benchmark) or
        decorators.GetEnabledAttributes(benchmark)):
      raise Exception(
          'Disabling or enabling telemetry benchmark with decorator detected. '
          'Please use StoryExpectations instead. Contact rnephew@ for more '
          'information. \nBenchmark: %s' % benchmark.Name())


def validate_story_names(benchmarks):
  for benchmark in benchmarks:
    if benchmark.Name() in CLUSTER_TELEMETRY_BENCHMARKS:
      continue
    b = benchmark()
    options = browser_options.BrowserFinderOptions()
    # tabset_repeat is needed for tab_switcher benchmarks.
    options.tabset_repeat = 1
    # test_path required for blink_perf benchmark in contrib/.
    options.test_path = ''
    # shared_prefs_file required for benchmarks in contrib/vr_benchmarks/
    options.shared_prefs_file = ''
    story_set = b.CreateStorySet(options)
    failed_stories = b.GetBrokenExpectations(story_set)
    assert not failed_stories, 'Incorrect story names: %s' % str(failed_stories)


def main(args):
  del args  # unused
  benchmarks = benchmark_finders.GetAllBenchmarks()
  validate_story_names(benchmarks)
  check_decorators(benchmarks)
  return 0
