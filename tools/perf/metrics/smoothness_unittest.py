# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import random
import unittest

from metrics import smoothness
from metrics import statistics
from metrics import rendering_stats
from telemetry.core.backends.chrome.tracing_backend import RawTraceResultImpl
from telemetry.core.backends.chrome.trace_result import TraceResult
from telemetry.page import page
from telemetry.page.page_measurement_results import PageMeasurementResults

SYNTHETIC_GESTURE_MARKER = 'SyntheticGestureController::running'


class MockTimer(object):
  """A mock timer class which can generate random durations.

  An instance of this class is used as a global timer to generate random
  durations for stats and consistent timestamps for all mock trace events.
  """
  def __init__(self):
    self.microseconds = 0

  def Advance(self, low=0, high=100000):
    duration = random.randint(low, high)
    self.microseconds += duration
    return duration


class MockFrame(object):
  """Mocks rendering, texture and latency stats for a single frame."""
  def __init__(self, mock_timer):
    """ Initialize the stats to random values """
    self.start = mock_timer.microseconds
    self.main_stats = {}
    self.impl_stats = {}
    self.texture_stats = {}
    self.latency_stats = {}
    self.main_stats['frame_count'] = 0
    self.main_stats['paint_time'] = mock_timer.Advance()
    self.main_stats['painted_pixel_count'] = random.randint(0, 2000000)
    self.main_stats['record_time'] = mock_timer.Advance()
    self.main_stats['recorded_pixel_count'] = random.randint(0, 2000000)
    self.impl_stats['frame_count'] = 1
    self.impl_stats['rasterize_time'] = mock_timer.Advance()
    self.impl_stats['rasterized_pixel_count'] = random.randint(0, 2000000)
    self.end = mock_timer.microseconds
    self.duration = self.end - self.start

  def AppendTraceEventForMainThreadStats(self, trace_events):
    """Appends a trace event with the main thread stats.

    The trace event is a dict with the following keys:
        'name',
        'tts' (thread timestamp),
        'pid' (process id),
        'ts' (timestamp),
        'cat' (category),
        'tid' (thread id),
        'ph' (phase),
        'args' (a dict with the key 'data').
    This is related to src/base/debug/trace_event.h.
    """
    event = {'name': 'MainThreadRenderingStats::IssueTraceEvent',
             'tts': self.end,
             'pid': 20978,
             'ts': self.end,
             'cat': 'benchmark',
             's': 't',
             'tid': 11,
             'ph': 'i',
             'args': {'data': self.main_stats}}
    trace_events.append(event)

  def AppendTraceEventForImplThreadStats(self, trace_events):
    """Appends a trace event with the impl thread stat."""
    event = {'name': 'ImplThreadRenderingStats::IssueTraceEvent',
             'tts': self.end,
             'pid': 20978,
             'ts': self.end,
             'cat': 'benchmark',
             's': 't',
             'tid': 11,
             'ph': 'i',
             'args': {'data': self.impl_stats}}
    trace_events.append(event)


class SmoothnessMetricUnitTest(unittest.TestCase):
  def testCalcResultsTraceEvents(self):
    # Make the test repeatable by seeding the random number generator
    # (which is used by the mock timer) with a constant number.
    random.seed(1234567)
    mock_timer = MockTimer()
    trace_events = []
    total_time_seconds = 0.0
    num_frames_sent = 0.0
    first_frame = True
    previous_frame_time = None
    # This list represents time differences between frames in milliseconds.
    expected_frame_times = []

    # Append start trace events for the timeline marker and gesture marker,
    # with some amount of time in between them.
    trace_events.append({'name': rendering_stats.RENDER_PROCESS_MARKER,
                         'tts': mock_timer.microseconds,
                         'args': {},
                         'pid': 20978,
                         'ts': mock_timer.microseconds,
                         'cat': 'webkit',
                         'tid': 11,
                         'ph': 'S',  # Phase: start.
                         'id': '0x12345'})
    mock_timer.Advance()
    trace_events.append({'name': SYNTHETIC_GESTURE_MARKER,
                         'tts': mock_timer.microseconds,
                         'args': {},
                         'pid': 20978,
                         'ts': mock_timer.microseconds,
                         'cat': 'webkit',
                         'tid': 11,
                         'ph': 'S',
                         'id': '0xabcde'})

    # Generate 100 random mock frames and append their trace events.
    for _ in xrange(0, 100):
      mock_frame = MockFrame(mock_timer)
      mock_frame.AppendTraceEventForMainThreadStats(trace_events)
      mock_frame.AppendTraceEventForImplThreadStats(trace_events)
      # Exclude the first frame, because it may have started before the
      # benchmark run.
      if not first_frame:
        total_time_seconds += mock_frame.duration / 1e6
        num_frames_sent += mock_frame.main_stats['frame_count']
        num_frames_sent += mock_frame.impl_stats['frame_count']
      first_frame = False
      current_frame_time = mock_timer.microseconds / 1000.0
      if previous_frame_time:
        difference = current_frame_time - previous_frame_time
        difference = round(difference, 2)
        expected_frame_times.append(difference)
      previous_frame_time = current_frame_time

    # Append finish trace events for the timeline and gesture markers, in the
    # reverse order from how they were added, with some time in between.
    trace_events.append({'name': SYNTHETIC_GESTURE_MARKER,
                         'tts': mock_timer.microseconds,
                         'args': {},
                         'pid': 20978,
                         'ts': mock_timer.microseconds,
                         'cat': 'webkit',
                         'tid': 11,
                         'ph': 'F',  # Phase: finish.
                         'id': '0xabcde'})
    mock_timer.Advance()
    trace_events.append({'name': rendering_stats.RENDER_PROCESS_MARKER,
                         'tts': mock_timer.microseconds,
                         'args': {},
                         'pid': 20978,
                         'ts': mock_timer.microseconds,
                         'cat': 'webkit',
                         'tid': 11,
                         'ph': 'F',
                         'id': '0x12345'})

    # Create a timeline object from the trace.
    trace_impl = RawTraceResultImpl(trace_events)
    trace_result = TraceResult(trace_impl)
    timeline = trace_result.AsTimelineModel()

    # Find the timeline marker and gesture marker in the timeline,
    # and create a RenderingStats object.
    render_process_marker = timeline.FindTimelineMarkers(
        rendering_stats.RENDER_PROCESS_MARKER)
    timeline_markers = timeline.FindTimelineMarkers(
        SYNTHETIC_GESTURE_MARKER)
    stats = rendering_stats.RenderingStats(
        render_process_marker, timeline_markers)

    # Make a results object and add results to it from the smoothness metric.
    results = PageMeasurementResults()
    results.WillMeasurePage(page.Page('http://foo.com/', None))
    smoothness_metric = smoothness.SmoothnessMetric(stats)
    smoothness_metric.AddResults(None, results)
    results.DidMeasurePage()

    self.assertEquals(
        expected_frame_times,
        results.page_results[0]['frame_times'].value)
    self.assertAlmostEquals(
        1000.0 * (total_time_seconds / num_frames_sent),
        results.page_results[0]['mean_frame_time'].value,
        places=2)

    # We don't verify the correctness of the discrepancy computation itself,
    # because we have a separate unit test for that purpose.
    self.assertAlmostEquals(
        statistics.FrameDiscrepancy(stats.frame_timestamps, True),
        results.page_results[0]['jank'].value,
        places=4)

    # We do not verify the correctness of Percentile here; Percentile should
    # have its own test.
    # The 17 here represents a threshold of 17 ms; this should match the value
    # in the smoothness metric.
    self.assertEquals(
        statistics.Percentile(expected_frame_times, 95.0) < 17.0,
        results.page_results[0]['mostly_smooth'].value)
