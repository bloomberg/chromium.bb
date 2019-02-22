# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import random
import unittest

from telemetry.timeline import async_slice
from telemetry.timeline import model
from telemetry.util import perf_tests_helper
from telemetry.web_perf import timeline_interaction_record as tir_module
from telemetry.web_perf.metrics import rendering_stats


class MockTimer(object):
  """A mock timer class which can generate random durations.

  An instance of this class is used as a global timer to generate random
  durations for stats and consistent timestamps for all mock trace events.
  The unit of time is milliseconds.
  """

  def __init__(self):
    self.milliseconds = 0

  def Advance(self, low=0.1, high=1):
    delta = random.uniform(low, high)
    self.milliseconds += delta
    return delta

  def AdvanceAndGet(self, low=0.1, high=1):
    self.Advance(low, high)
    return self.milliseconds


class MockVblankTimer(object):
  """A mock vblank timer class which can generate random durations.

  An instance of this class is used as a vblank timer to generate random
  durations for drm stats and consistent timeval for mock trace drm events.
  The unit of time is microseconds.
  """

  def __init__(self):
    self.microseconds = 200000000

  def TvAdvance(self, low=100, high=1000):
    delta = random.randint(low, high)
    self.microseconds += delta
    return delta

  def TvAdvanceAndGet(self, low=100, high=1000):
    self.TvAdvance(low, high)
    return self.microseconds


class ReferenceRenderingStats(object):
  """ Stores expected data for comparison with actual RenderingStats """

  def __init__(self):
    self.frame_timestamps = []
    self.frame_times = []

  def AppendNewRange(self):
    self.frame_timestamps.append([])
    self.frame_times.append([])


class ReferenceInputLatencyStats(object):
  """ Stores expected data for comparison with actual input latency stats """

  def __init__(self):
    self.input_event_latency = []
    self.input_event = []


def AddDrmEventFlipStats(mock_timer, vblank_timer, thread,
                         first_frame, ref_stats=None):
  """ Adds a random drm flip complete event.

  thread: The timeline model thread to which the event will be added.
  first_frame: Is this the first frame within the bounds of an action?
  ref_stats: A ReferenceRenderingStats object to record expected values.
  """
  # Create random data and timestamp for drm thread flip complete stats.
  vblank_timeval = vblank_timer.TvAdvanceAndGet()
  vblank_tv_sec = vblank_timeval / 1000000
  vblank_tv_usec = vblank_timeval % 1000000
  data = {'frame_count': 1,
          'vblank.tv_usec': vblank_tv_usec,
          'vblank.tv_sec': vblank_tv_sec}
  timestamp = mock_timer.AdvanceAndGet()

  # Add a slice with the event data to the given thread.
  thread.PushCompleteSlice(
      'benchmark,drm', 'DrmEventFlipComplete',
      timestamp, duration=0.0, thread_timestamp=None, thread_duration=None,
      args={'data': data})

  if not ref_stats:
    return

  # Add vblank timeval only if a frame was output.
  cur_timestamp = vblank_tv_sec * 1000.0 + vblank_tv_usec / 1000.0
  if not first_frame:
    # Add frame_time if this is not the first frame in within the bounds of an
    # action.
    prev_timestamp = ref_stats.frame_timestamps[-1][-1]
    ref_stats.frame_times[-1].append(cur_timestamp - prev_timestamp)
  ref_stats.frame_timestamps[-1].append(cur_timestamp)


def AddDisplayRenderingStats(mock_timer, thread, first_frame,
                             ref_stats=None):
  """ Adds a random display rendering stats event.

  thread: The timeline model thread to which the event will be added.
  first_frame: Is this the first frame within the bounds of an action?
  ref_stats: A ReferenceRenderingStats object to record expected values.
  """
  # Create random data and timestamp for main thread rendering stats.
  data = {'frame_count': 1}
  timestamp = mock_timer.AdvanceAndGet()

  # Add a slice with the event data to the given thread.
  thread.PushCompleteSlice(
      'benchmark', 'BenchmarkInstrumentation::DisplayRenderingStats',
      timestamp, duration=0.0, thread_timestamp=None, thread_duration=None,
      args={'data': data})

  if not ref_stats:
    return

  # Add timestamp only if a frame was output
  if not first_frame:
    # Add frame_time if this is not the first frame in within the bounds of an
    # action.
    prev_timestamp = ref_stats.frame_timestamps[-1][-1]
    ref_stats.frame_times[-1].append(timestamp - prev_timestamp)
  ref_stats.frame_timestamps[-1].append(timestamp)


def AddInputLatencyStats(mock_timer, start_thread, end_thread,
                         ref_latency_stats=None):
  """ Adds a random input latency stats event.

  start_thread: The start thread on which the async slice is added.
  end_thread: The end thread on which the async slice is ended.
  ref_latency_stats: A ReferenceInputLatencyStats object for expected values.
  """

  original_comp_time = mock_timer.AdvanceAndGet(2, 4) * 1000.0
  ui_comp_time = mock_timer.AdvanceAndGet(2, 4) * 1000.0
  begin_comp_time = mock_timer.AdvanceAndGet(2, 4) * 1000.0
  forward_comp_time = mock_timer.AdvanceAndGet(2, 4) * 1000.0
  end_comp_time = mock_timer.AdvanceAndGet(10, 20) * 1000.0

  data = {rendering_stats.ORIGINAL_COMP_NAME: {'time': original_comp_time},
          rendering_stats.UI_COMP_NAME: {'time': ui_comp_time},
          rendering_stats.BEGIN_COMP_NAME: {'time': begin_comp_time},
          rendering_stats.END_COMP_NAME: {'time': end_comp_time}}

  timestamp = mock_timer.AdvanceAndGet(2, 4)

  tracing_async_slice = async_slice.AsyncSlice(
      'benchmark', 'InputLatency', timestamp)

  async_sub_slice = async_slice.AsyncSlice(
      'benchmark', rendering_stats.GESTURE_SCROLL_UPDATE_EVENT_NAME, timestamp)
  async_sub_slice.args = {'data': data}
  async_sub_slice.parent_slice = tracing_async_slice
  async_sub_slice.start_thread = start_thread
  async_sub_slice.end_thread = end_thread

  tracing_async_slice.sub_slices.append(async_sub_slice)
  tracing_async_slice.start_thread = start_thread
  tracing_async_slice.end_thread = end_thread
  start_thread.AddAsyncSlice(tracing_async_slice)

  # Add scroll update latency info.
  scroll_update_data = {
      rendering_stats.BEGIN_SCROLL_UPDATE_COMP_NAME: {'time': begin_comp_time},
      rendering_stats.FORWARD_SCROLL_UPDATE_COMP_NAME:
          {'time': forward_comp_time},
      rendering_stats.END_COMP_NAME: {'time': end_comp_time}
  }

  scroll_async_slice = async_slice.AsyncSlice(
      'benchmark', 'InputLatency', timestamp)

  scroll_async_sub_slice = async_slice.AsyncSlice(
      'benchmark', rendering_stats.MAIN_THREAD_SCROLL_UPDATE_EVENT_NAME,
      timestamp)
  scroll_async_sub_slice.args = {'data': scroll_update_data}
  scroll_async_sub_slice.parent_slice = scroll_async_slice
  scroll_async_sub_slice.start_thread = start_thread
  scroll_async_sub_slice.end_thread = end_thread

  scroll_async_slice.sub_slices.append(scroll_async_sub_slice)
  scroll_async_slice.start_thread = start_thread
  scroll_async_slice.end_thread = end_thread
  start_thread.AddAsyncSlice(scroll_async_slice)

  if not ref_latency_stats:
    return

  ref_latency_stats.input_event.append(async_sub_slice)
  ref_latency_stats.input_event.append(scroll_async_sub_slice)
  ref_latency_stats.input_event_latency.append((
      rendering_stats.GESTURE_SCROLL_UPDATE_EVENT_NAME,
      (data[rendering_stats.END_COMP_NAME]['time'] -
       data[rendering_stats.ORIGINAL_COMP_NAME]['time']) / 1000.0))
  scroll_update_time = (
      scroll_update_data[rendering_stats.END_COMP_NAME]['time'] -
      scroll_update_data[rendering_stats.BEGIN_SCROLL_UPDATE_COMP_NAME]['time'])
  ref_latency_stats.input_event_latency.append((
      rendering_stats.MAIN_THREAD_SCROLL_UPDATE_EVENT_NAME,
      scroll_update_time / 1000.0))


class RenderingStatsUnitTest(unittest.TestCase):

  def testBothDrmAndDisplayStats(self):
    timeline = model.TimelineModel()
    timer = MockTimer()
    vblank_timer = MockVblankTimer()

    ref_stats = ReferenceRenderingStats()
    ref_stats.AppendNewRange()
    gpu = timeline.GetOrCreateProcess(pid=6)
    gpu.name = 'GPU Process'
    gpu_drm_thread = gpu.GetOrCreateThread(tid=61)
    renderer = timeline.GetOrCreateProcess(pid=2)
    browser = timeline.GetOrCreateProcess(pid=3)
    browser_main = browser.GetOrCreateThread(tid=31)
    browser_main.BeginSlice('webkit.console', 'ActionA',
                            timer.AdvanceAndGet(2, 4), '')
    vblank_timer.TvAdvance(2000, 4000)

    # Create drm flip stats and display rendering stats.
    for i in xrange(0, 10):
      first = (i == 0)
      AddDrmEventFlipStats(timer, vblank_timer, gpu_drm_thread,
                           first, ref_stats)
      timer.Advance(2, 4)
      vblank_timer.TvAdvance(2000, 4000)

    for i in xrange(0, 10):
      first = (i == 0)
      AddDisplayRenderingStats(timer, browser_main, first, None)
      timer.Advance(5, 10)
      vblank_timer.TvAdvance(5000, 10000)

    browser_main.EndSlice(timer.AdvanceAndGet())
    timer.Advance(2, 4)
    vblank_timer.TvAdvance(2000, 4000)

    browser.FinalizeImport()
    renderer.FinalizeImport()
    timeline_markers = timeline.FindTimelineMarkers(['ActionA'])
    records = [tir_module.TimelineInteractionRecord(e.name, e.start, e.end)
               for e in timeline_markers]
    stats = rendering_stats.RenderingStats(renderer, browser, gpu, records)

    # Compare rendering stats to reference - Only drm flip stats should
    # count
    self.assertEquals(stats.frame_timestamps, ref_stats.frame_timestamps)
    self.assertEquals(stats.frame_times, ref_stats.frame_times)

  def testDisplayStats(self):
    timeline = model.TimelineModel()
    timer = MockTimer()

    ref_stats = ReferenceRenderingStats()
    ref_stats.AppendNewRange()
    renderer = timeline.GetOrCreateProcess(pid=2)
    browser = timeline.GetOrCreateProcess(pid=3)
    browser_main = browser.GetOrCreateThread(tid=31)
    browser_main.BeginSlice('webkit.console', 'ActionA',
                            timer.AdvanceAndGet(2, 4), '')

    # Create display rendering stats.
    for i in xrange(0, 10):
      first = (i == 0)
      AddDisplayRenderingStats(timer, browser_main, first, ref_stats)
      timer.Advance(5, 10)

    browser_main.EndSlice(timer.AdvanceAndGet())
    timer.Advance(2, 4)

    browser.FinalizeImport()
    timeline_markers = timeline.FindTimelineMarkers(['ActionA'])
    records = [tir_module.TimelineInteractionRecord(e.name, e.start, e.end)
               for e in timeline_markers]
    stats = rendering_stats.RenderingStats(renderer, browser, None, records)

    # Compare rendering stats to reference - Only display stats should count
    self.assertEquals(stats.frame_timestamps, ref_stats.frame_timestamps)
    self.assertEquals(stats.frame_times, ref_stats.frame_times)

  def testRangeWithoutFrames(self):
    timer = MockTimer()
    timeline = model.TimelineModel()

    # Create a renderer process, with a main thread and impl thread.
    renderer = timeline.GetOrCreateProcess(pid=2)
    renderer_main = renderer.GetOrCreateThread(tid=21)

    renderer_main.BeginSlice('webkit.console', 'ActionA',
                             timer.AdvanceAndGet(2, 4), '')
    renderer_main.EndSlice(timer.AdvanceAndGet(2, 4))
    timer.Advance(2, 4)

    # Create Action B without any frames. This should trigger
    # NotEnoughFramesError when the RenderingStats object is created.
    renderer_main.BeginSlice('webkit.console', 'ActionB',
                             timer.AdvanceAndGet(2, 4), '')
    renderer_main.EndSlice(timer.AdvanceAndGet(2, 4))

    renderer.FinalizeImport()

    timeline_markers = timeline.FindTimelineMarkers(['ActionA', 'ActionB'])
    records = [tir_module.TimelineInteractionRecord(e.name, e.start, e.end)
               for e in timeline_markers]

    stats = rendering_stats.RenderingStats(renderer, None, None, records)
    self.assertEquals(0, len(stats.frame_timestamps[1]))

  def testFromTimeline(self):
    timeline = model.TimelineModel()

    # Create a browser process and a renderer process, and a main thread and
    # impl thread for each.
    browser = timeline.GetOrCreateProcess(pid=1)
    browser_main = browser.GetOrCreateThread(tid=11)
    renderer = timeline.GetOrCreateProcess(pid=2)

    timer = MockTimer()
    browser_ref_stats = ReferenceRenderingStats()

    browser_ref_stats.AppendNewRange()
    # Add display rendering stats.
    browser_main.BeginSlice('webkit.console', 'Action0',
                            timer.AdvanceAndGet(2, 4), '')
    for i in xrange(0, 10):
      first = (i == 0)
      AddDisplayRenderingStats(timer, browser_main, first, browser_ref_stats)
      timer.Advance(5, 10)
    browser_main.EndSlice(timer.AdvanceAndGet(2, 4))

    browser.FinalizeImport()

    timeline_markers = timeline.FindTimelineMarkers(['Action0'])
    records = [tir_module.TimelineInteractionRecord(e.name, e.start, e.end)
               for e in timeline_markers]
    stats = rendering_stats.RenderingStats(renderer, browser, None, records)

    # Compare rendering stats to reference.
    self.assertEquals(stats.frame_timestamps,
                      browser_ref_stats.frame_timestamps)
    self.assertEquals(stats.frame_times, browser_ref_stats.frame_times)

  def testInputLatencyFromTimeline(self):
    timeline = model.TimelineModel()

    # Create a browser process and a renderer process.
    browser = timeline.GetOrCreateProcess(pid=1)
    browser_main = browser.GetOrCreateThread(tid=11)
    renderer = timeline.GetOrCreateProcess(pid=2)
    renderer_main = renderer.GetOrCreateThread(tid=21)

    timer = MockTimer()
    ref_latency = ReferenceInputLatencyStats()

    # Create 10 input latency stats events for Action A.
    renderer_main.BeginSlice('webkit.console', 'ActionA',
                             timer.AdvanceAndGet(2, 4), '')
    for _ in xrange(0, 10):
      AddInputLatencyStats(timer, browser_main, renderer_main, ref_latency)
    renderer_main.EndSlice(timer.AdvanceAndGet(2, 4))

    # Create 5 input latency stats events not within any action.
    timer.Advance(2, 4)
    for _ in xrange(0, 5):
      AddInputLatencyStats(timer, browser_main, renderer_main, None)

    # Create 10 input latency stats events for Action B.
    renderer_main.BeginSlice('webkit.console', 'ActionB',
                             timer.AdvanceAndGet(2, 4), '')
    for _ in xrange(0, 10):
      AddInputLatencyStats(timer, browser_main, renderer_main, ref_latency)
    renderer_main.EndSlice(timer.AdvanceAndGet(2, 4))

    # Create 10 input latency stats events for Action A.
    renderer_main.BeginSlice('webkit.console', 'ActionA',
                             timer.AdvanceAndGet(2, 4), '')
    for _ in xrange(0, 10):
      AddInputLatencyStats(timer, browser_main, renderer_main, ref_latency)
    renderer_main.EndSlice(timer.AdvanceAndGet(2, 4))

    browser.FinalizeImport()
    renderer.FinalizeImport()

    latency_events = []

    timeline_markers = timeline.FindTimelineMarkers(
        ['ActionA', 'ActionB', 'ActionA'])
    records = [tir_module.TimelineInteractionRecord(e.name, e.start, e.end)
               for e in timeline_markers]
    for record in records:
      if record.GetBounds().is_empty:
        continue
      latency_events.extend(rendering_stats.GetLatencyEvents(
          browser, record.GetBounds()))

    self.assertEquals(latency_events, ref_latency.input_event)
    event_latency_result = rendering_stats.ComputeEventLatencies(latency_events)
    self.assertEquals(event_latency_result,
                      ref_latency.input_event_latency)

    stats = rendering_stats.RenderingStats(renderer, browser, None, records)
    self.assertEquals(
        perf_tests_helper.FlattenList(stats.input_event_latency),
        [latency for name, latency in ref_latency.input_event_latency
         if name != rendering_stats.MAIN_THREAD_SCROLL_UPDATE_EVENT_NAME])
    self.assertEquals(
        perf_tests_helper.FlattenList(stats.main_thread_scroll_latency),
        [latency for name, latency in ref_latency.input_event_latency
         if name == rendering_stats.MAIN_THREAD_SCROLL_UPDATE_EVENT_NAME])
    self.assertEquals(
        perf_tests_helper.FlattenList(stats.gesture_scroll_update_latency),
        [latency for name, latency in ref_latency.input_event_latency
         if name == rendering_stats.GESTURE_SCROLL_UPDATE_EVENT_NAME])
