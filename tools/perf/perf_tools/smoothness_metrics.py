# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
import os

from telemetry.core import util

class SmoothnessMetrics(object):
  def __init__(self, tab):
    self._tab = tab
    with open(
      os.path.join(os.path.dirname(__file__),
                   'smoothness_metrics.js')) as f:
      js = f.read()
      tab.ExecuteJavaScript(js)

  def Start(self):
    self._tab.ExecuteJavaScript(
        'window.__renderingStats = new __RenderingStats();'
        'window.__renderingStats.start()')

  def SetNeedsDisplayOnAllLayersAndStart(self):
    self._tab.ExecuteJavaScript(
        'chrome.gpuBenchmarking.setNeedsDisplayOnAllLayers();'
        'window.__renderingStats = new __RenderingStats();'
        'window.__renderingStats.start()')

  def Stop(self):
    self._tab.ExecuteJavaScript('window.__renderingStats.stop()')

  def BindToAction(self, action):
    # Make the scroll test start and stop measurement automatically.
    self._tab.ExecuteJavaScript(
        'window.__renderingStats = new __RenderingStats();')
    action.BindMeasurementJavaScript(self._tab,
                                     'window.__renderingStats.start();',
                                     'window.__renderingStats.stop();')

  @property
  def start_values(self):
    return self._tab.EvaluateJavaScript(
      'window.__renderingStats.getStartValues()')

  @property
  def end_values(self):
    return self._tab.EvaluateJavaScript(
      'window.__renderingStats.getEndValues()')

  @property
  def deltas(self):
    return self._tab.EvaluateJavaScript(
      'window.__renderingStats.getDeltas()')


def DivideIfPossibleOrZero(numerator, denominator):
  if denominator == 0:
    return 0
  return numerator / denominator

def CalcScrollResults(rendering_stats_deltas, results):
  num_frames_sent_to_screen = rendering_stats_deltas['numFramesSentToScreen']

  mean_frame_time_seconds = (
    rendering_stats_deltas['totalTimeInSeconds'] /
      float(num_frames_sent_to_screen))

  dropped_percent = (
    rendering_stats_deltas['droppedFrameCount'] /
    float(num_frames_sent_to_screen))

  num_impl_thread_scrolls = rendering_stats_deltas.get(
    'numImplThreadScrolls', 0)
  num_main_thread_scrolls = rendering_stats_deltas.get(
    'numMainThreadScrolls', 0)

  percent_impl_scrolled = DivideIfPossibleOrZero(
    float(num_impl_thread_scrolls),
    num_impl_thread_scrolls + num_main_thread_scrolls)

  num_layers = (
      rendering_stats_deltas.get('numLayersDrawn', 0) /
      float(num_frames_sent_to_screen))

  num_missing_tiles = (
      rendering_stats_deltas.get('numMissingTiles', 0) /
      float(num_frames_sent_to_screen))

  results.Add('mean_frame_time', 'ms', round(mean_frame_time_seconds * 1000, 3))
  results.Add('dropped_percent', '%', round(dropped_percent * 100, 1),
              data_type='unimportant')
  results.Add('percent_impl_scrolled', '%',
              round(percent_impl_scrolled * 100, 1),
              data_type='unimportant')
  results.Add('average_num_layers_drawn', '', round(num_layers, 1),
              data_type='unimportant')
  results.Add('average_num_missing_tiles', '', round(num_missing_tiles, 1),
              data_type='unimportant')

def CalcTextureUploadResults(rendering_stats_deltas, results):
  if (('totalCommitCount' not in rendering_stats_deltas)
      or rendering_stats_deltas['totalCommitCount'] == 0) :
    averageCommitTimeMs = 0
  else :
    averageCommitTimeMs = (
        1000 * rendering_stats_deltas['totalCommitTimeInSeconds'] /
        rendering_stats_deltas['totalCommitCount'])

  results.Add('texture_upload_count', 'count',
              rendering_stats_deltas.get('textureUploadCount', 0))
  results.Add('total_texture_upload_time', 'seconds',
              rendering_stats_deltas.get('totalTextureUploadTimeInSeconds', 0))
  results.Add('average_commit_time', 'ms', averageCommitTimeMs,
              data_type='unimportant')

def CalcFirstPaintTimeResults(results, tab):
  if tab.browser.is_content_shell:
    results.Add('first_paint', 'ms', 'unsupported')
    return

  tab.ExecuteJavaScript("""
      window.__rafFired = false;
      window.webkitRequestAnimationFrame(function() {
          window.__rafFired  = true;
      });
  """)
  util.WaitFor(lambda: tab.EvaluateJavaScript('window.__rafFired'), 60)

  first_paint_secs = tab.EvaluateJavaScript(
      'window.chrome.loadTimes().firstPaintTime - ' +
      'window.chrome.loadTimes().startLoadTime')

  results.Add('first_paint', 'ms', round(first_paint_secs * 1000, 1))

def CalcImageDecodingResults(rendering_stats_deltas, results):
  totalDeferredImageDecodeCount = rendering_stats_deltas.get(
      'totalDeferredImageDecodeCount', 0)
  totalDeferredImageCacheHitCount = rendering_stats_deltas.get(
      'totalDeferredImageCacheHitCount', 0)
  totalImageGatheringCount = rendering_stats_deltas.get(
      'totalImageGatheringCount', 0)
  totalDeferredImageDecodeTimeInSeconds = rendering_stats_deltas.get(
      'totalDeferredImageDecodeTimeInSeconds', 0)
  totalImageGatheringTimeInSeconds = rendering_stats_deltas.get(
      'totalImageGatheringTimeInSeconds', 0)

  averageImageGatheringTime = DivideIfPossibleOrZero(
      (totalImageGatheringTimeInSeconds * 1000), totalImageGatheringCount)

  results.Add('total_deferred_image_decode_count', 'count',
              totalDeferredImageDecodeCount,
              data_type='unimportant')
  results.Add('total_image_cache_hit_count', 'count',
              totalDeferredImageCacheHitCount,
              data_type='unimportant')
  results.Add('average_image_gathering_time', 'ms', averageImageGatheringTime,
              data_type='unimportant')
  results.Add('total_deferred_image_decoding_time', 'seconds',
              totalDeferredImageDecodeTimeInSeconds,
              data_type='unimportant')

def CalcAnalysisResults(rendering_stats_deltas, results):
  totalTilesAnalyzed = rendering_stats_deltas.get(
      'totalTilesAnalyzed', 0)
  solidColorTilesAnalyzed = rendering_stats_deltas.get(
      'solidColorTilesAnalyzed', 0)
  totalTileAnalysisTimeInSeconds = rendering_stats_deltas.get(
      'totalTileAnalysisTimeInSeconds', 0)

  averageAnalysisTimeMS = \
      1000 * DivideIfPossibleOrZero(totalTileAnalysisTimeInSeconds,
                                    totalTilesAnalyzed)

  results.Add('total_tiles_analyzed', 'count',
              totalTilesAnalyzed,
              data_type='unimportant')
  results.Add('solid_color_tiles_analyzed', 'count',
              solidColorTilesAnalyzed,
              data_type='unimportant')
  results.Add('average_tile_analysis_time', 'ms',
              averageAnalysisTimeMS,
              data_type='unimportant')

def CalcLatency(rendering_stats_deltas, count_name, total_latency_name,
                result_name, results):
  eventCount = rendering_stats_deltas.get(count_name, 0)
  totalLatencyInSeconds = rendering_stats_deltas.get(total_latency_name, 0)
  averageLatency = DivideIfPossibleOrZero(
      (totalLatencyInSeconds * 1000), eventCount)
  results.Add(result_name, 'ms', averageLatency, data_type='unimportant')

def CalcLatencyResults(rendering_stats_deltas, results):
  CalcLatency(rendering_stats_deltas, 'inputEventCount', 'totalInputLatency',
              'average_latency', results)
  CalcLatency(rendering_stats_deltas, 'touchUICount', 'totalTouchUILatency',
              'average_touch_ui_latency', results)
  CalcLatency(rendering_stats_deltas, 'touchAckedCount',
              'totalTouchAckedLatency',
              'average_touch_acked_latency',
              results)
  CalcLatency(rendering_stats_deltas, 'scrollUpdateCount',
              'totalScrollUpdateLatency',
              'average_scroll_update_latency', results)
