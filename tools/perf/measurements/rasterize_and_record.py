# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import logging
import time

from metrics import rendering_stats
from telemetry.page import page_measurement
from telemetry.page.perf_tests_helper import FlattenList
import telemetry.core.timeline.bounds as timeline_bounds
from telemetry.core.timeline.model import TimelineModel
from telemetry.core.timeline.model import MarkerMismatchError
from telemetry.core.timeline.model import MarkerOverlapError

TIMELINE_MARKER = 'RasterizeAndRecord'


class RasterizeAndRecord(page_measurement.PageMeasurement):
  def __init__(self):
    super(RasterizeAndRecord, self).__init__('', True)
    self._metrics = None
    self._compositing_features_enabled = False

  @classmethod
  def AddCommandLineArgs(cls, parser):
    parser.add_option('--raster-record-repeat', type='int',
                      default=20,
                      help='Repetitions in raster and record loops.'
                      'Higher values reduce variance, but can cause'
                      'instability (timeouts, event buffer overflows, etc.).')
    parser.add_option('--start-wait-time', type='float',
                      default=5,
                      help='Wait time before the benchmark is started '
                      '(must be long enought to load all content)')
    parser.add_option('--stop-wait-time', type='float',
                      default=15,
                      help='Wait time before measurement is taken '
                      '(must be long enough to render one frame)')

  def CustomizeBrowserOptions(self, options):
    # Run each raster task N times. This allows us to report the time for the
    # best run, effectively excluding cache effects and time when the thread is
    # de-scheduled.
    options.AppendExtraBrowserArgs([
        '--enable-gpu-benchmarking',
        '--slow-down-raster-scale-factor=%d' % options.raster_record_repeat,
        # Enable impl-side-painting. Current version of benchmark only works for
        # this mode.
        '--enable-impl-side-painting',
        '--force-compositing-mode',
        '--enable-threaded-compositing'
    ])

  def DidStartBrowser(self, browser):
    # Check if the we actually have threaded forced compositing enabled.
    system_info = browser.GetSystemInfo()
    if (system_info.gpu.feature_status
        and system_info.gpu.feature_status.get(
            'compositing', None) == 'enabled_force_threaded'):
      self._compositing_features_enabled = True

  def MeasurePage(self, page, tab, results):
    if not self._compositing_features_enabled:
      logging.warning('Warning: compositing feature status unknown or not '+
                      'forced and threaded. Skipping measurement.')
      return

    # Rasterize only what's visible.
    tab.ExecuteJavaScript(
        'chrome.gpuBenchmarking.setRasterizeOnlyVisibleContent();')

    # Wait until the page has loaded and come to a somewhat steady state.
    # Needs to be adjusted for every device (~2 seconds for workstation).
    time.sleep(self.options.start_wait_time)

    # Render one frame before we start gathering a trace. On some pages, the
    # first frame requested has more variance in the number of pixels
    # rasterized.
    tab.ExecuteJavaScript(
        'window.__rafFired = false;'
        'window.webkitRequestAnimationFrame(function() {'
          'chrome.gpuBenchmarking.setNeedsDisplayOnAllLayers();'
          'window.__rafFired  = true;'
        '});')

    time.sleep(self.options.stop_wait_time)
    tab.browser.StartTracing('webkit.console,benchmark', 60)

    tab.ExecuteJavaScript(
        'window.__rafFired = false;'
        'window.webkitRequestAnimationFrame(function() {'
          'chrome.gpuBenchmarking.setNeedsDisplayOnAllLayers();'
          'console.time("' + TIMELINE_MARKER + '");'
          'window.__rafFired  = true;'
        '});')
    # Wait until the frame was drawn.
    # Needs to be adjusted for every device and for different
    # raster_record_repeat counts.
    # TODO(ernstm): replace by call-back.
    time.sleep(self.options.stop_wait_time)
    tab.ExecuteJavaScript(
        'console.timeEnd("' + TIMELINE_MARKER + '")')

    tracing_timeline_data = tab.browser.StopTracing()
    timeline = TimelineModel(timeline_data=tracing_timeline_data)
    try:
      timeline_markers = timeline.FindTimelineMarkers(TIMELINE_MARKER)
    except (MarkerMismatchError, MarkerOverlapError) as e:
      raise page_measurement.MeasurementFailure(str(e))
    timeline_ranges = [ timeline_bounds.Bounds.CreateFromEvent(marker)
                        for marker in timeline_markers ]
    renderer_process = timeline.GetRendererProcessFromTab(tab)

    stats = rendering_stats.RenderingStats(
        renderer_process, timeline.browser_process, timeline_ranges)

    results.Add('rasterize_time', 'ms', max(FlattenList(stats.rasterize_times)))
    results.Add('record_time', 'ms', max(FlattenList(stats.record_times)))
    results.Add('rasterized_pixels', 'pixels',
                max(FlattenList(stats.rasterized_pixel_counts)))
    results.Add('recorded_pixels', 'pixels',
                max(FlattenList(stats.recorded_pixel_counts)))
