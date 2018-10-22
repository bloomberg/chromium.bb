# Copyright 2017 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
from core import perf_benchmark

import page_sets
from measurements import rendering
from telemetry import benchmark
from telemetry import story as story_module


@benchmark.Info(emails=['sadrul@chromium.org', 'vmiura@chromium.org'],
                documentation_url='https://bit.ly/rendering-benchmarks')
class RenderingDesktop(perf_benchmark.PerfBenchmark):

  test = rendering.Rendering
  SUPPORTED_PLATFORMS = [story_module.expectations.ALL_DESKTOP]

  @classmethod
  def Name(cls):
    return 'rendering.desktop'

  @classmethod
  def AddBenchmarkCommandLineArgs(cls, parser):
    parser.add_option('--scroll-forever', action='store_true',
                      help='If set, continuously scroll up and down forever. '
                           'This is useful for analysing scrolling behaviour '
                           'with tools such as perf.')

  def CreateStorySet(self, options):
    return page_sets.RenderingStorySet(platform='desktop')


@benchmark.Info(emails=['sadrul@chromium.org', 'vmiura@chromium.org'],
                documentation_url='https://bit.ly/rendering-benchmarks')
class RenderingMobile(perf_benchmark.PerfBenchmark):

  test = rendering.Rendering
  SUPPORTED_PLATFORMS = [story_module.expectations.ALL_MOBILE]

  @classmethod
  def Name(cls):
    return 'rendering.mobile'

  @classmethod
  def AddBenchmarkCommandLineArgs(cls, parser):
    parser.add_option('--scroll-forever', action='store_true',
                      help='If set, continuously scroll up and down forever. '
                           'This is useful for analysing scrolling behaviour '
                           'with tools such as perf.')

  def CreateStorySet(self, options):
    return page_sets.RenderingStorySet(platform='mobile')
