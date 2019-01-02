# Copyright 2018 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import optparse

from telemetry import benchmark as b_module
from telemetry.internal.browser import browser_options


def GetBenchmarkStorySet(benchmark):
  if not isinstance(benchmark, b_module.Benchmark):
    raise ValueError(
      '|benchmark| must be an instace of telemetry.benchmark.Benchmark class. '
      'Instead found object of type: %s' % type(benchmark))
  options = browser_options.BrowserFinderOptions()
  # Add default values for any extra commandline options
  # provided by the benchmark.
  parser = optparse.OptionParser()
  before, _ = parser.parse_args([])
  benchmark.AddBenchmarkCommandLineArgs(parser)
  after, _ = parser.parse_args([])
  for extra_option in dir(after):
    if extra_option not in dir(before):
      setattr(options, extra_option, getattr(after, extra_option))
  return benchmark.CreateStorySet(options)


def GetBenchmarkStoryNames(benchmark):
  story_list = []
  for story in GetBenchmarkStorySet(benchmark):
    if story.name not in story_list:
      story_list.append(story.name)
  return sorted(story_list)


def GetStoryTags(benchmark):
  tags = set()
  for story in GetBenchmarkStorySet(benchmark):
    tags.update(story.tags)
  return sorted(list(tags))
