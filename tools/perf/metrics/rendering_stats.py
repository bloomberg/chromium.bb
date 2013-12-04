# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from operator import attrgetter


class RenderingStats(object):
  def __init__(self, renderer_process, timeline_markers):
    """
    Utility class for extracting rendering statistics from the timeline (or
    other loggin facilities), and providing them in a common format to classes
    that compute benchmark metrics from this data.

    Stats can either be numbers, or lists of numbers. Classes that calculate
    metrics from the stats must be able to handle both cases. The length of
    different list stats may vary.

    All *_time values are measured in milliseconds.
    """
    assert(len(timeline_markers) > 0)
    self.renderer_process = renderer_process

    self.frame_timestamps = []
    self.frame_times = []
    self.paint_time = []
    self.painted_pixel_count = []
    self.record_time = []
    self.recorded_pixel_count = []
    self.rasterize_time = []
    self.rasterized_pixel_count = []

    for marker in timeline_markers:
      self.initMainThreadStatsFromTimeline(marker.start,
                                           marker.start+marker.duration)
      self.initImplThreadStatsFromTimeline(marker.start,
                                           marker.start+marker.duration)

  def initMainThreadStatsFromTimeline(self, start, end):
    event_name = 'BenchmarkInstrumentation::MainThreadRenderingStats'
    events = []
    for event in self.renderer_process.IterAllSlicesOfName(event_name):
      if event.start >= start and event.end <= end:
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
        self.frame_timestamps.append(
            event.start)
        if not first_frame:
          self.frame_times.append(round(self.frame_timestamps[-1] -
                                        self.frame_timestamps[-2], 2))
        first_frame = False
      self.paint_time.append(1000.0 *
          event.args['data']['paint_time'])
      self.painted_pixel_count.append(
          event.args['data']['painted_pixel_count'])
      self.record_time.append(1000.0 *
          event.args['data']['record_time'])
      self.recorded_pixel_count.append(
          event.args['data']['recorded_pixel_count'])

  def initImplThreadStatsFromTimeline(self, start, end):
    event_name = 'BenchmarkInstrumentation::ImplThreadRenderingStats'
    events = []
    for event in self.renderer_process.IterAllSlicesOfName(event_name):
      if event.start >= start and event.end <= end:
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
        self.frame_timestamps.append(
            event.start)
        if not first_frame:
          self.frame_times.append(round(self.frame_timestamps[-1] -
                                        self.frame_timestamps[-2], 2))
        first_frame = False
      self.rasterize_time.append(1000.0 *
          event.args['data']['rasterize_time'])
      self.rasterized_pixel_count.append(
          event.args['data']['rasterized_pixel_count'])
