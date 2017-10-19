#!/usr/bin/env python
# Copyright 2017 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Script to check validity of StoryExpectations."""

import argparse
import json
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


def GetDisabledStories(benchmarks):
  # Creates a dictionary of the format:
  # {
  #   'benchmark_name1' : {
  #     'story_1': [
  #       {'conditions': conditions, 'reason': reason},
  #       ...
  #     ],
  #     ...
  #   },
  #   ...
  # }
  disables = {}
  for benchmark in benchmarks:
    name = benchmark.Name()
    disables[name] = {}
    expectations = benchmark().GetExpectations().AsDict()['stories']
    for story in expectations:
      for conditions, reason in  expectations[story]:
        if not disables[name].get(story):
          disables[name][story] = []
          conditions_str = [str(a) for a in conditions]
        disables[name][story].append((conditions_str, reason))
  return disables


def main(args):
  parser = argparse.ArgumentParser(
      description=('Tests if disabled stories exist.'))
  parser.add_argument(
      '--list', action='store_true', default=False,
      help=('Prints list of disabled stories.'))
  options = parser.parse_args(args)
  benchmarks = benchmark_finders.GetAllBenchmarks()

  if options.list:
    stories = GetDisabledStories(benchmarks)
    print json.dumps(stories, sort_keys=True, indent=4, separators=(',', ': '))
  else:
    validate_story_names(benchmarks)
    check_decorators(benchmarks)
  return 0
