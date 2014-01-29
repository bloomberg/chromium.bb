# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from operator import attrgetter


class RenderingStats(object):
  def __init__(self, renderer_process, timeline_ranges):
    """
    Utility class for extracting rendering statistics from the timeline (or
    other loggin facilities), and providing them in a common format to classes
    that compute benchmark metrics from this data.

    Stats are lists of lists of numbers. The outer list stores one list per
    timeline range.

    All *_time values are measured in milliseconds.
    """
    assert(len(timeline_ranges) > 0)
    self.renderer_process = renderer_process

    self.frame_timestamps = []
    self.frame_times = []
    self.paint_times = []
    self.painted_pixel_counts = []
    self.record_times = []
    self.recorded_pixel_counts = []
    self.rasterize_times = []
    self.rasterized_pixel_counts = []

    for timeline_range in timeline_ranges:
      self.frame_timestamps.append([])
      self.frame_times.append([])
      self.paint_times.append([])
      self.painted_pixel_counts.append([])
      self.record_times.append([])
      self.recorded_pixel_counts.append([])
      self.rasterize_times.append([])
      self.rasterized_pixel_counts.append([])

      if timeline_range.is_empty:
        continue
      self.initMainThreadStatsFromTimeline(timeline_range)
      self.initImplThreadStatsFromTimeline(timeline_range)

  def initMainThreadStatsFromTimeline(self, timeline_range):
    event_name = 'BenchmarkInstrumentation::MainThreadRenderingStats'
    events = []
    for event in self.renderer_process.IterAllSlicesOfName(event_name):
      if event.start >= timeline_range.min and event.end <= timeline_range.max:
        if 'data' not in event.args:
          continue
        events.append(event)
    events.sort(key=attrgetter('start'))

    first_frame = True
    for event in events:
      frame_count = event.args['data']['frame_count']
      if frame_count > 1:
        raise ValueError, 'trace contains multi-frame render stats'
      if frame_count == 1:
        self.frame_timestamps[-1].append(
            event.start)
        if not first_frame:
          self.frame_times[-1].append(round(self.frame_timestamps[-1] -
                                            self.frame_timestamps[-2], 2))
        first_frame = False
      self.paint_times[-1].append(1000.0 *
          event.args['data']['paint_time'])
      self.painted_pixel_counts[-1].append(
          event.args['data']['painted_pixel_count'])
      self.record_times[-1].append(1000.0 *
          event.args['data']['record_time'])
      self.recorded_pixel_counts[-1].append(
          event.args['data']['recorded_pixel_count'])

  def initImplThreadStatsFromTimeline(self, timeline_range):
    event_name = 'BenchmarkInstrumentation::ImplThreadRenderingStats'
    events = []
    for event in self.renderer_process.IterAllSlicesOfName(event_name):
      if event.start >= timeline_range.min and event.end <= timeline_range.max:
        if 'data' not in event.args:
          continue
        events.append(event)
    events.sort(key=attrgetter('start'))

    first_frame = True
    for event in events:
      frame_count = event.args['data']['frame_count']
      if frame_count > 1:
        raise ValueError, 'trace contains multi-frame render stats'
      if frame_count == 1:
        self.frame_timestamps[-1].append(
            event.start)
        if not first_frame:
          self.frame_times[-1].append(round(self.frame_timestamps[-1][-1] -
                                            self.frame_timestamps[-1][-2], 2))
        first_frame = False
      self.rasterize_times[-1].append(1000.0 *
          event.args['data']['rasterize_time'])
      self.rasterized_pixel_counts[-1].append(
          event.args['data']['rasterized_pixel_count'])
