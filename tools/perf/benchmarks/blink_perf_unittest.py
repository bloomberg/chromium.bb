# Copyright 2017 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
import os
import unittest

from telemetry import decorators
from telemetry import story
from telemetry.page import page as page_module
from telemetry.testing import options_for_unittests
from telemetry.testing import page_test_test_case
from telemetry.timeline import async_slice
from telemetry.timeline import model as model_module


from benchmarks import blink_perf


class BlinkPerfTest(page_test_test_case.PageTestTestCase):
  _BLINK_PERF_TEST_DATA_DIR = os.path.join(os.path.dirname(__file__),
      '..', '..', '..', 'third_party', 'WebKit', 'PerformanceTests',
      'TestData')

  _BLINK_PERF_RESOURCES_DIR = os.path.join(os.path.dirname(__file__),
      '..', '..', '..', 'third_party', 'WebKit', 'PerformanceTests',
      'resources')
  def setUp(self):
    self._options = options_for_unittests.GetCopy()
    # pylint: disable=protected-access
    self._measurement = blink_perf._BlinkPerfMeasurement()
    # pylint: enable=protected-access

  def _CreateStorySetForTestFile(self, test_file_name):
    story_set = story.StorySet(base_dir=self._BLINK_PERF_TEST_DATA_DIR,
        serving_dirs={self._BLINK_PERF_TEST_DATA_DIR,
                      self._BLINK_PERF_RESOURCES_DIR})
    page = page_module.Page('file://' + test_file_name, story_set,
        base_dir=story_set.base_dir)
    story_set.AddStory(page)
    return story_set

  @decorators.Disabled('win')  # crbug.com/715822
  def testBlinkPerfTracingMetricsForMeasureTime(self):
    results = self.RunMeasurement(measurement=self._measurement,
        ps=self._CreateStorySetForTestFile('append-child-measure-time.html'),
        options=self._options)
    self.assertFalse(results.failures)
    self.assertEquals(len(results.FindAllTraceValues()), 1)

    frame_view_layouts = results.FindAllPageSpecificValuesNamed(
        'FrameView::layout')
    self.assertEquals(len(frame_view_layouts), 1)
    self.assertGreater(frame_view_layouts[0].GetRepresentativeNumber, 0.1)

    update_layout_trees = results.FindAllPageSpecificValuesNamed(
        'UpdateLayoutTree')
    self.assertEquals(len(update_layout_trees), 1)
    self.assertGreater(update_layout_trees[0].GetRepresentativeNumber, 0.1)

  @decorators.Disabled('android')  # crbug.com/715685
  def testBlinkPerfTracingMetricsForMeasureFrameTime(self):
    results = self.RunMeasurement(measurement=self._measurement,
        ps=self._CreateStorySetForTestFile(
            'color-changes-measure-frame-time.html'),
        options=self._options)
    self.assertFalse(results.failures)
    self.assertEquals(len(results.FindAllTraceValues()), 1)

    frame_view_prepaints = results.FindAllPageSpecificValuesNamed(
        'FrameView::prePaint')
    self.assertEquals(len(frame_view_prepaints), 1)
    self.assertGreater(frame_view_prepaints[0].GetRepresentativeNumber, 0.1)

    frame_view_painttrees = results.FindAllPageSpecificValuesNamed(
        'FrameView::paintTree')
    self.assertEquals(len(frame_view_painttrees), 1)
    self.assertGreater(frame_view_painttrees[0].GetRepresentativeNumber, 0.1)


# pylint: disable=protected-access
# This is needed for testing _ComputeTraceEventsThreadTimeForBlinkPerf method.
class ComputeTraceEventsMetricsForBlinkPerfTest(unittest.TestCase):

  def _AddBlinkTestSlice(self, renderer_thread, start, end):
    s = async_slice.AsyncSlice(
        'blink', 'blink_perf.runTest',
        timestamp=start, duration=end - start, start_thread=renderer_thread,
        end_thread=renderer_thread)
    renderer_thread.AddAsyncSlice(s)

  def testTraceEventMetricsSingleBlinkTest(self):
    model = model_module.TimelineModel()
    renderer_main = model.GetOrCreateProcess(1).GetOrCreateThread(2)
    renderer_main.name = 'CrRendererMain'

    # Set up a main thread model that looks like:
    #   [          blink_perf.run_test                ]
    #   |     [ foo ]          [  bar ]      [        baz     ]
    #   |     |     |          |      |      |        |       |
    #   100   120   140        400    420    500      550     600
    #             |                |                  |
    # CPU dur:    15               18                 70
    #
    self._AddBlinkTestSlice(renderer_main, 100, 550)

    renderer_main.BeginSlice('blink', 'foo', 120, 122)
    renderer_main.EndSlice(140, 137)

    renderer_main.BeginSlice('blink', 'bar', 400, 402)
    renderer_main.EndSlice(420, 420)

    # Since this "baz" slice has CPU duration = 70ms, wall-time duration = 100ms
    # & its overalapped wall-time with "blink_perf.run_test" is 50 ms, its
    # overlapped CPU time with "blink_perf.run_test" is
    # 50 * 70 / 100 = 35ms.
    renderer_main.BeginSlice('blink', 'baz', 500, 520)
    renderer_main.EndSlice(600, 590)

    self.assertEquals(
        blink_perf._ComputeTraceEventsThreadTimeForBlinkPerf(
            renderer_main, ['foo', 'bar', 'baz']),
        {'foo': [15], 'bar': [18], 'baz': [35]})


  def testTraceEventMetricsMultiBlinkTest(self):
    model = model_module.TimelineModel()
    renderer_main = model.GetOrCreateProcess(1).GetOrCreateThread(2)
    renderer_main.name = 'CrRendererMain'

    # Set up a main thread model that looks like:
    #   [          blink_perf.run_test    ]         [ blink_perf.run_test  ]
    #   |     [ foo ]          [  bar ]   |     [   |    foo         ]     |
    #   |     |     |          |      |   |     |   |     |          |     |
    #   100   120   140        400    420 440   500 520              600   640
    #             |                |                      |
    # CPU dur:    15               18                     40
    #
    self._AddBlinkTestSlice(renderer_main, 100, 440)
    self._AddBlinkTestSlice(renderer_main, 520, 640)

    renderer_main.BeginSlice('blink', 'foo', 120, 122)
    renderer_main.EndSlice(140, 137)

    renderer_main.BeginSlice('blink', 'bar', 400, 402)
    renderer_main.EndSlice(420, 420)

    # Since this "foo" slice has CPU duration = 40ms, wall-time duration = 100ms
    # & its overalapped wall-time with "blink_perf.run_test" is 80 ms, its
    # overlapped CPU time with "blink_perf.run_test" is
    # 80 * 40 / 100 = 32ms.
    renderer_main.BeginSlice('blink', 'foo', 500, 520)
    renderer_main.EndSlice(600, 560)

    self.assertEquals(
        blink_perf._ComputeTraceEventsThreadTimeForBlinkPerf(
            renderer_main, ['foo', 'bar', 'baz']),
        {'foo': [15, 32], 'bar': [18, 0], 'baz': [0, 0]})

  def testTraceEventMetricsNoThreadTimeAvailable(self):
    model = model_module.TimelineModel()
    renderer_main = model.GetOrCreateProcess(1).GetOrCreateThread(2)
    renderer_main.name = 'CrRendererMain'

    # Set up a main thread model that looks like:
    #   [          blink_perf.run_test                ]
    #   |     [ foo ]          [  bar ]               |
    #   |     |     |          |      |               |
    #   100   120   140        400    420             550
    #             |                |
    # CPU dur:    None             None
    #
    self._AddBlinkTestSlice(renderer_main, 100, 550)

    renderer_main.BeginSlice('blink', 'foo', 120)
    renderer_main.EndSlice(140)

    renderer_main.BeginSlice('blink', 'bar', 400)
    renderer_main.EndSlice(420)

    self.assertEquals(
        blink_perf._ComputeTraceEventsThreadTimeForBlinkPerf(
            renderer_main, ['foo', 'bar']),
        {'foo': [20], 'bar': [20]})
