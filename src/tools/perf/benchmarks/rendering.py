# Copyright 2017 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
from core import perf_benchmark

import page_sets
from telemetry import benchmark
from telemetry import story as story_module
from telemetry.timeline import chrome_trace_category_filter
from telemetry.web_perf import timeline_based_measurement

@benchmark.Info(emails=['sadrul@chromium.org', 'vmiura@chromium.org'],
                documentation_url='https://bit.ly/rendering-benchmarks',
                component='Internals>GPU>Metrics')
class RenderingDesktop(perf_benchmark.PerfBenchmark):

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

  def SetExtraBrowserOptions(self, options):
    options.AppendExtraBrowserArgs('--enable-gpu-benchmarking')
    options.AppendExtraBrowserArgs('--touch-events=enabled')

  def CreateCoreTimelineBasedMeasurementOptions(self):
    category_filter = chrome_trace_category_filter.CreateLowOverheadFilter()
    options = timeline_based_measurement.Options(category_filter)
    options.config.chrome_trace_config.EnableUMAHistograms(
        'Event.Latency.ScrollBegin.Touch.TimeToScrollUpdateSwapBegin4',
        'Event.Latency.ScrollUpdate.Touch.TimeToScrollUpdateSwapBegin4')
    options.SetTimelineBasedMetrics(['renderingMetric', 'umaMetric'])
    return options


@benchmark.Info(emails=['sadrul@chromium.org', 'vmiura@chromium.org'],
                documentation_url='https://bit.ly/rendering-benchmarks',
                component='Internals>GPU>Metrics')
class RenderingMobile(perf_benchmark.PerfBenchmark):

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

  def SetExtraBrowserOptions(self, options):
    options.AppendExtraBrowserArgs('--enable-gpu-benchmarking')
    options.AppendExtraBrowserArgs('--touch-events=enabled')

  def CreateCoreTimelineBasedMeasurementOptions(self):
    category_filter = chrome_trace_category_filter.CreateLowOverheadFilter()
    options = timeline_based_measurement.Options(category_filter)
    options.config.enable_platform_display_trace = True
    options.config.chrome_trace_config.EnableUMAHistograms(
        'Event.Latency.ScrollBegin.Touch.TimeToScrollUpdateSwapBegin4',
        'Event.Latency.ScrollUpdate.Touch.TimeToScrollUpdateSwapBegin4')
    options.SetTimelineBasedMetrics(['renderingMetric', 'umaMetric'])
    return options
