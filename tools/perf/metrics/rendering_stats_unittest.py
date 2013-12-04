# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import random
import unittest

from metrics.rendering_stats import RenderingStats
from telemetry.core.timeline import model


class MockTimer(object):
  """A mock timer class which can generate random durations.

  An instance of this class is used as a global timer to generate random
  durations for stats and consistent timestamps for all mock trace events.
  The unit of time is milliseconds.
  """
  def __init__(self):
    self.milliseconds = 0

  def Get(self):
    return self.milliseconds

  def Advance(self, low=0, high=1):
    delta = random.uniform(low, high)
    self.milliseconds += delta
    return delta


class ReferenceRenderingStats(object):
  """ Stores expected data for comparison with actual RenderingStats """
  def __init__(self):
    self.frame_timestamps = []
    self.frame_times = []
    self.paint_time = []
    self.painted_pixel_count = []
    self.record_time = []
    self.recorded_pixel_count = []
    self.rasterize_time = []
    self.rasterized_pixel_count = []


def AddMainThreadRenderingStats(mock_timer, thread, first_frame,
                                ref_stats = None):
  """ Adds a random main thread rendering stats event.

  thread: The timeline model thread to which the event will be added.
  first_frame: Is this the first frame within the bounds of an action?
  ref_stats: A ReferenceRenderingStats object to record expected values.
  """
  # Create randonm data and timestap for main thread rendering stats.
  data = { 'frame_count': 0,
           'paint_time': 0.0,
           'painted_pixel_count': 0,
           'record_time': mock_timer.Advance(2, 4) / 1000.0,
           'recorded_pixel_count': 3000*3000 }
  timestamp = mock_timer.Get()

  # Add a slice with the event data to the given thread.
  thread.PushCompleteSlice(
      'benchmark', 'BenchmarkInstrumentation::MainThreadRenderingStats',
      timestamp, duration=0.0, thread_timestamp=None, thread_duration=None,
      args={'data': data})

  if not ref_stats:
    return

  # Add timestamp only if a frame was output
  if data['frame_count'] == 1:
    if not first_frame:
      # Add frame_time if this is not the first frame in within the bounds of an
      # action.
      prev_timestamp = ref_stats.frame_timestamps[-1]
      ref_stats.frame_times.append(round(timestamp - prev_timestamp, 2))
    ref_stats.frame_timestamps.append(timestamp)

  ref_stats.paint_time.append(data['paint_time'] * 1000.0)
  ref_stats.painted_pixel_count.append(data['painted_pixel_count'])
  ref_stats.record_time.append(data['record_time'] * 1000.0)
  ref_stats.recorded_pixel_count.append(data['recorded_pixel_count'])


def AddImplThreadRenderingStats(mock_timer, thread, first_frame,
                                ref_stats = None):
  """ Adds a random impl thread rendering stats event.

  thread: The timeline model thread to which the event will be added.
  first_frame: Is this the first frame within the bounds of an action?
  ref_stats: A ReferenceRenderingStats object to record expected values.
  """
  # Create randonm data and timestap for impl thread rendering stats.
  data = { 'frame_count': 1,
           'rasterize_time': mock_timer.Advance(5, 10) / 1000.0,
           'rasterized_pixel_count': 1280*720 }
  timestamp = mock_timer.Get()

  # Add a slice with the event data to the given thread.
  thread.PushCompleteSlice(
      'benchmark', 'BenchmarkInstrumentation::ImplThreadRenderingStats',
      timestamp, duration=0.0, thread_timestamp=None, thread_duration=None,
      args={'data': data})

  if not ref_stats:
    return

  # Add timestamp only if a frame was output
  if data['frame_count'] == 1:
    if not first_frame:
      # Add frame_time if this is not the first frame in within the bounds of an
      # action.
      prev_timestamp = ref_stats.frame_timestamps[-1]
      ref_stats.frame_times.append(round(timestamp - prev_timestamp, 2))
    ref_stats.frame_timestamps.append(timestamp)

  ref_stats.rasterize_time.append(data['rasterize_time'] * 1000.0)
  ref_stats.rasterized_pixel_count.append(data['rasterized_pixel_count'])


class RenderingStatsUnitTest(unittest.TestCase):
  def testFromTimeline(self):
    timeline = model.TimelineModel()

    # Create a browser process and a renderer process, and a main thread and
    # impl thread for each.
    browser = timeline.GetOrCreateProcess(pid = 1)
    browser_main = browser.GetOrCreateThread(tid = 11)
    browser_compositor = browser.GetOrCreateThread(tid = 12)
    renderer = timeline.GetOrCreateProcess(pid = 2)
    renderer_main = renderer.GetOrCreateThread(tid = 21)
    renderer_compositor = renderer.GetOrCreateThread(tid = 22)

    timer = MockTimer()
    ref_stats = ReferenceRenderingStats()

    # Create 10 main and impl rendering stats events for Action A.
    timer.Advance()
    renderer_main.BeginSlice('webkit.console', 'ActionA', timer.Get(), '')
    for i in xrange(0, 10):
      first = (i == 0)
      AddMainThreadRenderingStats(timer, renderer_main, first, ref_stats)
      AddImplThreadRenderingStats(timer, renderer_compositor, first, ref_stats)
      AddMainThreadRenderingStats(timer, browser_main, first, None)
      AddImplThreadRenderingStats(timer, browser_compositor, first, None)
    renderer_main.EndSlice(timer.Get())

    # Create 5 main and impl rendering stats events not within any action.
    for i in xrange(0, 5):
      first = (i == 0)
      AddMainThreadRenderingStats(timer, renderer_main, first, None)
      AddImplThreadRenderingStats(timer, renderer_compositor, first, None)
      AddMainThreadRenderingStats(timer, browser_main, first, None)
      AddImplThreadRenderingStats(timer, browser_compositor, first, None)

    # Create 10 main and impl rendering stats events for Action B.
    timer.Advance()
    renderer_main.BeginSlice('webkit.console', 'ActionB', timer.Get(), '')
    for i in xrange(0, 10):
      first = (i == 0)
      AddMainThreadRenderingStats(timer, renderer_main, first, ref_stats)
      AddImplThreadRenderingStats(timer, renderer_compositor, first, ref_stats)
      AddMainThreadRenderingStats(timer, browser_main, first, None)
      AddImplThreadRenderingStats(timer, browser_compositor, first, None)
    renderer_main.EndSlice(timer.Get())

    # Create 10 main and impl rendering stats events for Action A.
    timer.Advance()
    renderer_main.BeginSlice('webkit.console', 'ActionA', timer.Get(), '')
    for i in xrange(0, 10):
      first = (i == 0)
      AddMainThreadRenderingStats(timer, renderer_main, first, ref_stats)
      AddImplThreadRenderingStats(timer, renderer_compositor, first, ref_stats)
      AddMainThreadRenderingStats(timer, browser_main, first, None)
      AddImplThreadRenderingStats(timer, browser_compositor, first, None)
    renderer_main.EndSlice(timer.Get())

    renderer_main.FinalizeImport()
    renderer_compositor.FinalizeImport()

    timeline_markers = timeline.FindTimelineMarkers(
        ['ActionA', 'ActionB', 'ActionA'])
    stats = RenderingStats(renderer, timeline_markers)

    # Compare rendering stats to reference.
    self.assertEquals(stats.frame_timestamps, ref_stats.frame_timestamps)
    self.assertEquals(stats.frame_times, ref_stats.frame_times)
    self.assertEquals(stats.rasterize_time, ref_stats.rasterize_time)
    self.assertEquals(stats.rasterized_pixel_count,
                      ref_stats.rasterized_pixel_count)
    self.assertEquals(stats.paint_time, ref_stats.paint_time)
    self.assertEquals(stats.painted_pixel_count, ref_stats.painted_pixel_count)
    self.assertEquals(stats.record_time, ref_stats.record_time)
    self.assertEquals(stats.recorded_pixel_count,
                      ref_stats.recorded_pixel_count)
