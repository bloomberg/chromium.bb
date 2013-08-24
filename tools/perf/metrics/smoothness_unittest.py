# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
import unittest
import random

from metrics import smoothness
from metrics.gpu_rendering_stats import GpuRenderingStats
from telemetry.page import page
from telemetry.page.page_measurement_results import PageMeasurementResults
from telemetry.core.chrome.tracing_backend import RawTraceResultImpl
from telemetry.core.chrome.trace_result import TraceResult

class MockTimer(object):
  """ An instance of this class is used as a global timer to generate
      random durations for stats and consistent timestamps for all mock trace
      events.
  """
  def __init__(self):
    self.microseconds = 0

  def Advance(self, low = 0, high = 100000):
    duration = random.randint(low, high)
    self.microseconds += duration
    return duration

class MockFrame(object):
  """ This class mocks rendering stats, texture stats and latency stats for
      a single frame
  """
  def __init__(self, mock_timer):
    """ Initialize the stats to random values """
    self.start = mock_timer.microseconds
    self.main_stats = {}
    self.impl_stats = {}
    self.texture_stats = {}
    self.latency_stats = {}
    self.main_stats["animation_frame_count"] = 0
    self.main_stats['screen_frame_count'] = 0
    self.main_stats['paint_time'] = mock_timer.Advance()
    self.main_stats['record_time'] = mock_timer.Advance()
    self.main_stats['commit_time'] = mock_timer.Advance()
    self.main_stats['commit_count'] = 1
    self.main_stats['painted_pixel_count'] = random.randint(0, 2000000)
    self.main_stats['recorded_pixel_count'] = random.randint(0, 2000000)
    self.main_stats['image_gathering_count'] = random.randint(0, 100)
    self.main_stats['image_gathering_time'] = mock_timer.Advance()
    self.impl_stats['screen_frame_count'] = 1
    self.impl_stats['dropped_frame_count'] = random.randint(0, 4)
    self.impl_stats['rasterize_time'] = mock_timer.Advance()
    self.impl_stats['rasterize_time_for_now_bins_on_pending_tree'] = \
        mock_timer.Advance()
    self.impl_stats['best_rasterize_time'] = mock_timer.Advance()
    self.impl_stats['rasterized_pixel_count'] = random.randint(0, 2000000)
    self.impl_stats['impl_thread_scroll_count'] = random.randint(0, 4)
    self.impl_stats['main_thread_scroll_count'] = random.randint(0, 4)
    self.impl_stats['drawn_layer_count'] = random.randint(0, 10)
    self.impl_stats['missing_tile_count'] = random.randint(0, 10000)
    self.impl_stats['deferred_image_decode_count'] = random.randint(0, 100)
    self.impl_stats['deferred_image_cache_hit_count'] = random.randint(0, 50)
    self.impl_stats['tile_analysis_count'] = random.randint(0, 10000)
    self.impl_stats['solid_color_tile_analysis_count'] = random.randint(0, 1000)
    self.impl_stats['deferred_image_decode_time'] = mock_timer.Advance()
    self.impl_stats['tile_analysis_time'] = mock_timer.Advance()
    self.texture_stats['texture_upload_count'] = random.randint(0, 1000)
    self.texture_stats['texture_upload_time'] = mock_timer.Advance()
    self.latency_stats['input_event_count'] = random.randint(0, 20)
    self.latency_stats['input_event_latency'] = mock_timer.Advance()
    self.latency_stats['touch_ui_count'] = random.randint(0, 20)
    self.latency_stats['touch_ui_latency'] = mock_timer.Advance()
    self.latency_stats['touch_acked_count'] = random.randint(0, 20)
    self.latency_stats['touch_acked_latency'] = mock_timer.Advance()
    self.latency_stats['scroll_update_count'] = random.randint(0, 20)
    self.latency_stats['scroll_update_latency'] = mock_timer.Advance()
    self.end = mock_timer.microseconds
    self.duration = self.end - self.start

  def AddToRenderingStats(self, rendering_stats):
    """ Add the stats for this frame to a mock RenderingStats object (i.e. a
        dictionary)
    """
    rs = rendering_stats
    rs['totalTimeInSeconds'] += self.duration / 1e6
    rs['numFramesSentToScreen'] += (self.main_stats['screen_frame_count'] +
                                    self.impl_stats['screen_frame_count'])
    rs['droppedFrameCount'] += self.impl_stats['dropped_frame_count']
    rs['numImplThreadScrolls'] += self.impl_stats['impl_thread_scroll_count']
    rs['numMainThreadScrolls'] += self.impl_stats['main_thread_scroll_count']
    rs['numLayersDrawn'] += self.impl_stats['drawn_layer_count']
    rs['numMissingTiles'] += self.impl_stats['missing_tile_count']
    rs['textureUploadCount'] += self.texture_stats['texture_upload_count']
    rs['totalTextureUploadTimeInSeconds'] += \
        self.texture_stats['texture_upload_time']
    rs['totalCommitCount'] += self.main_stats['commit_count']
    rs['totalCommitTimeInSeconds'] += self.main_stats['commit_time']
    rs['totalDeferredImageDecodeCount'] += self.impl_stats[
        'deferred_image_decode_count']
    rs['totalDeferredImageDecodeTimeInSeconds'] += self.impl_stats[
        'deferred_image_decode_time']
    rs['totalDeferredImageCacheHitCount'] += self.impl_stats[
        'deferred_image_cache_hit_count']
    rs['totalImageGatheringCount'] += self.main_stats['image_gathering_count']
    rs['totalImageGatheringTimeInSeconds'] += self.main_stats[
        'image_gathering_time']
    rs['totalTilesAnalyzed'] += self.impl_stats['tile_analysis_count']
    rs['totalTileAnalysisTimeInSeconds'] += self.impl_stats[
        'tile_analysis_time']
    rs['solidColorTilesAnalyzed'] += self.impl_stats[
        'solid_color_tile_analysis_count']
    rs['inputEventCount'] += self.latency_stats['input_event_count']
    rs['totalInputLatency'] += self.latency_stats['input_event_latency']
    rs['touchUICount'] += self.latency_stats['touch_ui_count']
    rs['totalTouchUILatency'] += self.latency_stats['touch_ui_latency']
    rs['touchAckedCount'] += self.latency_stats['touch_acked_count']
    rs['totalTouchAckedLatency'] += self.latency_stats['touch_acked_latency']
    rs['scrollUpdateCount'] += self.latency_stats['scroll_update_count']
    rs['totalScrollUpdateLatency'] += self.latency_stats[
        'scroll_update_latency']

  def AppendTraceEventForMainThreadStats(self, trace):
    """ Append a trace event with the main thread stats to trace """
    event = {'name': 'MainThreadRenderingStats::IssueTraceEvent',
             'tts': self.end,
             'pid': 20978,
             'ts': self.end,
             'cat': 'benchmark',
             's': 't',
             'tid': 11,
             'ph': 'i',
             'args': {'data': self.main_stats}}
    trace.append(event)

  def AppendTraceEventForImplThreadStats(self, trace):
    """ Append a trace event with the impl thread stat to trace """
    event = {'name': 'ImplThreadRenderingStats::IssueTraceEvent',
             'tts': self.end,
             'pid': 20978,
             'ts': self.end,
             'cat': 'benchmark',
             's': 't',
             'tid': 11,
             'ph': 'i',
             'args': {'data': self.impl_stats}}
    trace.append(event)

class SmoothnessMetricsUnitTest(unittest.TestCase):
  def FindTimelineMarker(self, timeline):
    events = [s for
              s in timeline.GetAllEventsOfName(
                  smoothness.TIMELINE_MARKER)
              if s.parent_slice == None]
    if len(events) != 1:
      raise LookupError, 'timeline marker not found'
    return events[0]

  def testCalcResultsTraceEvents(self):
    # Make the test repeatable by seeding the random number generator
    random.seed(1234567)
    mock_timer = MockTimer()
    trace = []
    rendering_stats = {
        'totalTimeInSeconds': 0.0,
        'numFramesSentToScreen': 0.0,
        'droppedFrameCount': 0.0,
        'numImplThreadScrolls': 0.0,
        'numMainThreadScrolls': 0.0,
        'numLayersDrawn': 0.0,
        'numMissingTiles': 0.0,
        'textureUploadCount': 0.0,
        'totalTextureUploadTimeInSeconds': 0.0,
        'totalCommitCount': 0.0,
        'totalCommitTimeInSeconds': 0.0,
        'totalDeferredImageDecodeCount': 0.0,
        'totalDeferredImageDecodeTimeInSeconds': 0.0,
        'totalDeferredImageCacheHitCount': 0.0,
        'totalImageGatheringCount': 0.0,
        'totalImageGatheringTimeInSeconds': 0.0,
        'totalTilesAnalyzed': 0.0,
        'totalTileAnalysisTimeInSeconds': 0.0,
        'solidColorTilesAnalyzed': 0.0,
        'inputEventCount': 0.0,
        'totalInputLatency': 0.0,
        'touchUICount': 0.0,
        'totalTouchUILatency': 0.0,
        'touchAckedCount': 0.0,
        'totalTouchAckedLatency': 0.0,
        'scrollUpdateCount': 0.0,
        'totalScrollUpdateLatency': 0.0}

    # Append a start trace event for the timeline marker
    trace.append({'name': smoothness.TIMELINE_MARKER,
                  'tts': mock_timer.microseconds,
                  'args': {},
                  'pid': 20978,
                  'ts': mock_timer.microseconds,
                  'cat': 'webkit',
                  'tid': 11,
                  'ph': 'S',
                  'id': '0xafb37737e249e055'})
    # Generate 100 random mock frames, append their trace events, and accumulate
    # stats in rendering_stats.
    for _ in xrange(0, 100):
      mock_frame = MockFrame(mock_timer)
      mock_frame.AppendTraceEventForMainThreadStats(trace)
      mock_frame.AppendTraceEventForImplThreadStats(trace)
      mock_frame.AddToRenderingStats(rendering_stats)
    # Append finish trace event for timeline marker
    trace.append({'name': smoothness.TIMELINE_MARKER,
                  'tts': mock_timer.microseconds,
                  'args': {},
                  'pid': 20978,
                  'ts': mock_timer.microseconds,
                  'cat': 'webkit',
                  'tid': 11,
                  'ph': 'F',
                  'id': '0xafb37737e249e055'})

    # Create timeline object from the trace
    trace_impl = RawTraceResultImpl(trace)
    trace_result = TraceResult(trace_impl)
    timeline = trace_result.AsTimelineModel()


    timeline_marker = self.FindTimelineMarker(timeline)
    stats = GpuRenderingStats(timeline_marker,
                              rendering_stats,
                              True)

    res = PageMeasurementResults()
    res.WillMeasurePage(page.Page('http://foo.com/', None))
    smoothness.CalcResults(stats, res)
    res.DidMeasurePage()

    rs = rendering_stats

    # Scroll Results
    self.assertAlmostEquals(
        round(rs['totalTimeInSeconds'] / rs['numFramesSentToScreen'] * 1000.0,
              3),
        res.page_results[0]['mean_frame_time'].value, 2)
    self.assertAlmostEquals(
        round(rs['droppedFrameCount'] / rs['numFramesSentToScreen'] * 100.0, 1),
        res.page_results[0]['dropped_percent'].value)
    self.assertAlmostEquals(
        round(rs['numImplThreadScrolls'] / (rs['numImplThreadScrolls'] +
                                            rs['numMainThreadScrolls']) * 100.0,
              1),
        res.page_results[0]['percent_impl_scrolled'].value)
    self.assertAlmostEquals(
        round(rs['numLayersDrawn'] / rs['numFramesSentToScreen'], 1),
        res.page_results[0]['average_num_layers_drawn'].value)
    self.assertAlmostEquals(
        round(rs['numMissingTiles'] / rs['numFramesSentToScreen'], 1),
        res.page_results[0]['average_num_missing_tiles'].value)

    # Texture Upload Results
    self.assertAlmostEquals(
        round(rs['totalCommitTimeInSeconds'] / rs['totalCommitCount'] * 1000.0,
              3),
        res.page_results[0]['average_commit_time'].value)
    self.assertEquals(
        rs['textureUploadCount'],
        res.page_results[0]['texture_upload_count'].value)
    self.assertEquals(
        rs['totalTextureUploadTimeInSeconds'],
        res.page_results[0]['total_texture_upload_time'].value)

    # Image Decoding Results
    self.assertEquals(
        rs['totalDeferredImageDecodeCount'],
        res.page_results[0]['total_deferred_image_decode_count'].value)
    self.assertEquals(
        rs['totalDeferredImageCacheHitCount'],
        res.page_results[0]['total_image_cache_hit_count'].value)
    self.assertAlmostEquals(
        round(rs['totalImageGatheringTimeInSeconds'] /
              rs['totalImageGatheringCount'] * 1000.0, 3),
        res.page_results[0]['average_image_gathering_time'].value)
    self.assertEquals(
        rs['totalDeferredImageDecodeTimeInSeconds'],
        res.page_results[0]['total_deferred_image_decoding_time'].value)

    # Tile Analysis Results
    self.assertEquals(
        rs['totalTilesAnalyzed'],
        res.page_results[0]['total_tiles_analyzed'].value)
    self.assertEquals(
        rs['solidColorTilesAnalyzed'],
        res.page_results[0]['solid_color_tiles_analyzed'].value)
    self.assertAlmostEquals(
        round(rs['totalTileAnalysisTimeInSeconds'] /
              rs['totalTilesAnalyzed'] * 1000.0, 3),
        res.page_results[0]['average_tile_analysis_time'].value)

    # Latency Results
    self.assertAlmostEquals(
        round(rs['totalInputLatency'] / rs['inputEventCount'] * 1000.0, 3),
        res.page_results[0]['average_latency'].value)
    self.assertAlmostEquals(
        round(rs['totalTouchUILatency'] / rs['touchUICount'] * 1000.0, 3),
        res.page_results[0]['average_touch_ui_latency'].value)
    self.assertAlmostEquals(
        round(rs['totalTouchAckedLatency'] / rs['touchAckedCount'] * 1000.0, 3),
        res.page_results[0]['average_touch_acked_latency'].value)
    self.assertAlmostEquals(
        round(rs['totalScrollUpdateLatency'] / rs['scrollUpdateCount'] * 1000.0,
              3),
        res.page_results[0]['average_scroll_update_latency'].value)
