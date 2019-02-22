# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import unittest

from telemetry.internal.results import page_test_results
from telemetry.page import page as page_module
from telemetry.web_perf.metrics import smoothness


class _MockRenderingStats(object):

  stats = ['refresh_period', 'frame_timestamps', 'frame_times', 'paint_times',
           'painted_pixel_counts', 'record_times', 'recorded_pixel_counts',
           'input_event_latency', 'frame_queueing_durations',
           'main_thread_scroll_latency', 'gesture_scroll_update_latency']

  def __init__(self, **kwargs):
    self.input_event_latency = None  # to avoid pylint no-member error
    self.errors = {}
    for stat in self.stats:
      value = kwargs[stat] if stat in kwargs else None
      setattr(self, stat, value)


#pylint: disable=protected-access
class SmoothnessMetricUnitTest(unittest.TestCase):

  def setUp(self):
    self.metric = smoothness.SmoothnessMetric()
    self.page = page_module.Page('file://blank.html', name='blank.html')
    self.good_timestamps = [[10, 20], [30, 40, 50]]
    self.not_enough_frames_timestamps = [[10], [20, 30, 40]]

  def testPopulateResultsFromStats(self):
    stats = _MockRenderingStats()
    for stat in _MockRenderingStats.stats:
      # Just set fake data for all of the relevant arrays of stats typically
      # found in a RenderingStats object.
      setattr(stats, stat, [[10, 20], [30, 40, 50]])
    results = page_test_results.PageTestResults()
    results.WillRunPage(self.page)
    self.metric._PopulateResultsFromStats(results, stats, False)
    current_page_run = results.current_page_run
    self.assertTrue(current_page_run.ok)
    expected_values_count = 6
    self.assertEquals(expected_values_count, len(current_page_run.values))

  def testHasEnoughFrames(self):
    # This list will pass since every sub-array has at least 2 frames.
    has_enough_frames = self.metric._HasEnoughFrames(self.good_timestamps)
    self.assertTrue(has_enough_frames)

  def testHasEnoughFramesWithNotEnoughFrames(self):
    # This list will fail since the first sub-array only has a single frame.
    has_enough_frames = self.metric._HasEnoughFrames(
        self.not_enough_frames_timestamps)
    self.assertFalse(has_enough_frames)

  def testComputeLatencyMetric(self):
    stats = _MockRenderingStats(frame_timestamps=self.good_timestamps,
                                input_event_latency=[[10, 20], [30, 40, 50]])
    raw = self.metric._ComputeLatencyMetric(
        self.page, stats, 'input_event_latency', stats.input_event_latency)
    self.assertEquals([10, 20, 30, 40, 50], raw.values)

  def testComputeLatencyMetricWithMissingData(self):
    stats = _MockRenderingStats(frame_timestamps=self.good_timestamps,
                                input_event_latency=[[], []])
    value = self.metric._ComputeLatencyMetric(
        self.page, stats, 'input_event_latency', stats.input_event_latency)
    self.assertEquals(None, value)

  def testComputeLatencyMetricWithNotEnoughFrames(self):
    stats = _MockRenderingStats(
        frame_timestamps=self.not_enough_frames_timestamps,
        input_event_latency=[[], []])
    raw = self.metric._ComputeLatencyMetric(
        self.page, stats, 'input_event_latency', stats.input_event_latency)
    self.assertEquals(None, raw.values)

  def testComputeGestureScrollUpdateLatencies(self):
    stats = _MockRenderingStats(
        frame_timestamps=self.good_timestamps,
        gesture_scroll_update_latency=[[10, 20], [30, 40, 50]])
    gesture_value = self.metric._ComputeFirstGestureScrollUpdateLatencies(
        self.page, stats)
    self.assertEquals([10, 30], gesture_value.values)

  def testComputeGestureScrollUpdateLatenciesWithMissingData(self):
    stats = _MockRenderingStats(
        frame_timestamps=self.good_timestamps,
        gesture_scroll_update_latency=[[], []])
    value = self.metric._ComputeFirstGestureScrollUpdateLatencies(
        self.page, stats)
    self.assertEquals(None, value.values)

  def testComputeGestureScrollUpdateLatenciesWithNotEnoughFrames(self):
    stats = _MockRenderingStats(
        frame_timestamps=self.not_enough_frames_timestamps,
        gesture_scroll_update_latency=[[10, 20], [30, 40, 50]])
    gesture_value = self.metric._ComputeFirstGestureScrollUpdateLatencies(
        self.page, stats)
    self.assertEquals(None, gesture_value.values)
    self.assertEquals(smoothness.NOT_ENOUGH_FRAMES_MESSAGE,
                      gesture_value.none_value_reason)

  def testComputeQueueingDuration(self):
    stats = _MockRenderingStats(frame_timestamps=self.good_timestamps,
                                frame_queueing_durations=[[10, 20], [30, 40]])
    list_of_scalar_values = self.metric._ComputeQueueingDuration(self.page,
                                                                 stats)
    self.assertEquals([10, 20, 30, 40], list_of_scalar_values.values)

  def testComputeQueueingDurationWithMissingData(self):
    stats = _MockRenderingStats(frame_timestamps=self.good_timestamps,
                                frame_queueing_durations=[[], []])
    list_of_scalar_values = self.metric._ComputeQueueingDuration(
        self.page, stats)
    self.assertEquals(None, list_of_scalar_values.values)
    self.assertEquals('No frame queueing durations recorded.',
                      list_of_scalar_values.none_value_reason)

  def testComputeQueueingDurationWithMissingDataAndErrorValue(self):
    stats = _MockRenderingStats(frame_timestamps=self.good_timestamps,
                                frame_queueing_durations=[[], []])
    stats.errors['frame_queueing_durations'] = (
        'Current chrome version does not support the queueing delay metric.')
    list_of_scalar_values = self.metric._ComputeQueueingDuration(
        self.page, stats)
    self.assertEquals(None, list_of_scalar_values.values)
    self.assertEquals(
        'Current chrome version does not support the queueing delay metric.',
        list_of_scalar_values.none_value_reason)

  def testComputeQueueingDurationWithNotEnoughFrames(self):
    stats = _MockRenderingStats(
        frame_timestamps=self.not_enough_frames_timestamps,
        frame_queueing_durations=[[10, 20], [30, 40, 50]])
    list_of_scalar_values = self.metric._ComputeQueueingDuration(self.page,
                                                                 stats)
    self.assertEquals(None, list_of_scalar_values.values)
    self.assertEquals(smoothness.NOT_ENOUGH_FRAMES_MESSAGE,
                      list_of_scalar_values.none_value_reason)

  def testComputeFrameTimeMetric(self):
    stats = _MockRenderingStats(frame_timestamps=self.good_timestamps,
                                frame_times=[[10, 20], [30, 40, 50]])
    frame_times_value, percentage_smooth_value = (
        self.metric._ComputeDisplayFrameTimeMetric(self.page, stats))
    self.assertEquals([10, 20, 30, 40, 50], frame_times_value.values)
    self.assertEquals(20, percentage_smooth_value.value)

  def testComputeFrameTimeMetricWithNotEnoughFrames2(self):
    stats = _MockRenderingStats(
        frame_timestamps=self.not_enough_frames_timestamps,
        frame_times=[[10, 20], [30, 40, 50]])
    frame_times_value, percentage_smooth_value = (
        self.metric._ComputeDisplayFrameTimeMetric(self.page, stats))
    self.assertEquals(None, frame_times_value.values)
    self.assertEquals(smoothness.NOT_ENOUGH_FRAMES_MESSAGE,
                      frame_times_value.none_value_reason)
    self.assertEquals(None, percentage_smooth_value.value)
    self.assertEquals(smoothness.NOT_ENOUGH_FRAMES_MESSAGE,
                      percentage_smooth_value.none_value_reason)
