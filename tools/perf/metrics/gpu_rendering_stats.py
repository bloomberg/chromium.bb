# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

class GpuRenderingStats(object):
  def __init__(self, timeline_marker, rendering_stats_deltas,
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
    self.renderer_process = timeline_marker.start_thread.parent
    self.start = timeline_marker.start
    self.end = self.start + timeline_marker.duration

    self.total_time = (self.end - self.start) / 1000.0
    self.animation_frame_count = []
    self.screen_frame_count = []
    self.paint_time = []
    self.record_time = []
    self.commit_time = []
    self.commit_count = []
    self.painted_pixel_count = []
    self.recorded_pixel_count = []
    self.image_gathering_count = []
    self.image_gathering_time = []
    self.dropped_frame_count = []
    self.rasterize_time = []
    self.rasterize_time_for_now_bins_on_pending_tree = []
    self.best_rasterize_time = []
    self.rasterized_pixel_count = []
    self.impl_thread_scroll_count = []
    self.main_thread_scroll_count = []
    self.drawn_layer_count = []
    self.missing_tile_count = []
    self.deferred_image_decode_count = []
    self.deferred_image_cache_hit_count = []
    self.tile_analysis_count = []
    self.solid_color_tile_analysis_count = []
    self.deferred_image_decode_time = []
    self.tile_analysis_time = []
    self.texture_upload_count = []
    self.texture_upload_time = []
    self.input_event_count = []
    self.input_event_latency = []
    self.touch_ui_count = []
    self.touch_ui_latency = []
    self.touch_acked_count = []
    self.touch_acked_latency = []
    self.scroll_update_count = []
    self.scroll_update_latency = []

    if used_gpu_benchmarking:
      self.initMainThreadStatsFromTimeline()
      self.initImplThreadStatsFromTimeline()
    else:
      self.initFrameCountsFromRenderingStats(rendering_stats_deltas)
    self.initTextureStatsFromRenderingStats(rendering_stats_deltas)
    self.initLatencyStatsFromRenderingStats(rendering_stats_deltas)

  def initFrameCountsFromRenderingStats(self, rs):
    self.animation_frame_count = rs.get('numAnimationFrames', 0)
    self.screen_frame_count = rs.get('numFramesSentToScreen', 0)
    self.dropped_frame_count = rs.get('droppedFrameCount', 0)

  def initTextureStatsFromRenderingStats(self, rs):
    self.texture_upload_count = rs.get('textureUploadCount', 0)
    self.texture_upload_time = rs.get('totalTextureUploadTimeInSeconds', 0)

  def initLatencyStatsFromRenderingStats(self, rs):
    self.input_event_count = rs.get('inputEventCount', 0)
    self.input_event_latency = rs.get('totalInputLatency', 0)
    self.touch_ui_count = rs.get('touchUICount', 0)
    self.touch_ui_latency = rs.get('totalTouchUILatency', 0)
    self.touch_acked_count = rs.get('touchAckedCount', 0)
    self.touch_acked_latency = rs.get('totalTouchAckedLatency', 0)
    self.scroll_update_count = rs.get('scrollUpdateCount', 0)
    self.scroll_update_latency = rs.get('totalScrollUpdateLatency', 0)

  def initMainThreadStatsFromTimeline(self):
    for event in self.renderer_process.IterAllSlicesOfName(
        'MainThreadRenderingStats::IssueTraceEvent'):
      if event.start >= self.start and event.end <= self.end:
        if 'data' not in event.args:
          continue
        self.animation_frame_count.append(
            event.args['data']['animation_frame_count'])
        self.screen_frame_count.append(
            event.args['data']['screen_frame_count'])
        self.paint_time.append(
            event.args['data']['paint_time'])
        self.record_time.append(
            event.args['data']['record_time'])
        self.commit_time.append(
            event.args['data']['commit_time'])
        self.commit_count.append(
            event.args['data']['commit_count'])
        self.painted_pixel_count.append(
            event.args['data']['painted_pixel_count'])
        self.recorded_pixel_count.append(
            event.args['data']['recorded_pixel_count'])
        self.image_gathering_count.append(
            event.args['data']['image_gathering_count'])
        self.image_gathering_time.append(
            event.args['data']['image_gathering_time'])

  def initImplThreadStatsFromTimeline(self):
    for event in self.renderer_process.IterAllSlicesOfName(
        'ImplThreadRenderingStats::IssueTraceEvent'):
      if event.start >= self.start and event.end <= self.end:
        if 'data' not in event.args:
          continue
        self.screen_frame_count.append(
            event.args['data']['screen_frame_count'])
        self.dropped_frame_count.append(
            event.args['data']['dropped_frame_count'])
        self.rasterize_time.append(
            event.args['data']['rasterize_time'])
        self.rasterize_time_for_now_bins_on_pending_tree.append(
            event.args['data']['rasterize_time_for_now_bins_on_pending_tree'])
        self.best_rasterize_time.append(
            event.args['data']['best_rasterize_time'])
        self.rasterized_pixel_count.append(
            event.args['data']['rasterized_pixel_count'])
        self.impl_thread_scroll_count.append(
            event.args['data']['impl_thread_scroll_count'])
        self.main_thread_scroll_count.append(
            event.args['data']['main_thread_scroll_count'])
        self.drawn_layer_count.append(
            event.args['data']['drawn_layer_count'])
        self.missing_tile_count.append(
            event.args['data']['missing_tile_count'])
        self.deferred_image_decode_count.append(
            event.args['data']['deferred_image_decode_count'])
        self.deferred_image_cache_hit_count.append(
            event.args['data']['deferred_image_cache_hit_count'])
        self.tile_analysis_count.append(
            event.args['data']['tile_analysis_count'])
        self.solid_color_tile_analysis_count.append(
            event.args['data']['solid_color_tile_analysis_count'])
        self.deferred_image_decode_time.append(
            event.args['data']['deferred_image_decode_time'])
        self.tile_analysis_time.append(
            event.args['data']['tile_analysis_time'])
