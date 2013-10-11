# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

class RenderingStats(object):
  def __init__(self, smoothness_marker, gesture_marker, rendering_stats_deltas,
               used_gpu_benchmarking):
    """
    Utility class for extracting rendering statistics from the timeline (or
    other loggin facilities), and providing them in a common format to classes
    that compute benchmark metrics from this data.

    Stats can either be numbers, or lists of numbers. Classes that calculate
    metrics from the stats must be able to handle both cases. The length of
    different list stats may vary.

    All *_time values are measured in seconds.
    """
    self.renderer_process = smoothness_marker.start_thread.parent
    self.start = gesture_marker.start
    self.end = self.start + gesture_marker.duration

    self.total_time = (self.end - self.start) / 1000.0
    self.frame_count = []
    self.frame_timestamps = []
    self.paint_time = []
    self.painted_pixel_count = []
    self.record_time = []
    self.recorded_pixel_count = []
    self.rasterize_time = []
    self.rasterized_pixel_count = []

    if used_gpu_benchmarking:
      self.initMainThreadStatsFromTimeline()
      self.initImplThreadStatsFromTimeline()
    else:
      self.initFrameCountsFromRenderingStats(rendering_stats_deltas)

  def initFrameCountsFromRenderingStats(self, rs):
    # TODO(ernstm): remove numFramesSentToScreen when RenderingStats
    # cleanup CL was picked up by the reference build.
    if 'frameCount' in rs:
      self.frame_count = rs.get('frameCount', 0)
    else:
      self.frame_count = rs.get('numFramesSentToScreen', 0)

  def initMainThreadStatsFromTimeline(self):
    # TODO(ernstm): Remove when CL with new event names was rolled into
    # reference build.
    event_name = 'BenchmarkInstrumentation::MainThreadRenderingStats'
    for event in self.renderer_process.IterAllSlicesOfName(
        'MainThreadRenderingStats::IssueTraceEvent'):
      event_name = 'MainThreadRenderingStats::IssueTraceEvent'
    for event in self.renderer_process.IterAllSlicesOfName(event_name):
      if event.start >= self.start and event.end <= self.end:
        if 'data' not in event.args:
          continue
        # TODO(ernstm): remove screen_frame_count when RenderingStats
        # cleanup CL was picked up by the reference build.
        if 'frame_count' in event.args['data']:
          frame_count = event.args['data']['frame_count']
        else:
          frame_count = event.args['data']['screen_frame_count']
        self.frame_count.append(frame_count)
        if frame_count > 1:
          raise ValueError, 'trace contains multi-frame render stats'
        if frame_count == 1:
          self.frame_timestamps.append(
              event.start)
        self.paint_time.append(
            event.args['data']['paint_time'])
        self.painted_pixel_count.append(
            event.args['data']['painted_pixel_count'])
        self.record_time.append(
            event.args['data']['record_time'])
        self.recorded_pixel_count.append(
            event.args['data']['recorded_pixel_count'])

  def initImplThreadStatsFromTimeline(self):
    # TODO(ernstm): Remove when CL with new event names was rolled into
    # reference build.
    event_name = 'BenchmarkInstrumentation::ImplThreadRenderingStats'
    for event in self.renderer_process.IterAllSlicesOfName(
        'ImplThreadRenderingStats::IssueTraceEvent'):
      event_name = 'ImplThreadRenderingStats::IssueTraceEvent'
    for event in self.renderer_process.IterAllSlicesOfName(event_name):
      if event.start >= self.start and event.end <= self.end:
        if 'data' not in event.args:
          continue
        # TODO(ernstm): remove screen_frame_count when RenderingStats
        # cleanup CL was picked up by the reference build.
        if 'frame_count' in event.args['data']:
          frame_count = event.args['data']['frame_count']
        else:
          frame_count = event.args['data']['screen_frame_count']
        self.frame_count.append(frame_count)
        if frame_count > 1:
          raise ValueError, 'trace contains multi-frame render stats'
        if frame_count == 1:
          self.frame_timestamps.append(
              event.start)
        self.rasterize_time.append(
            event.args['data']['rasterize_time'])
        self.rasterized_pixel_count.append(
            event.args['data']['rasterized_pixel_count'])
