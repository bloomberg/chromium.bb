# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import time

from metrics import smoothness
from metrics.rendering_stats import RenderingStats
from telemetry.page import page_measurement

class RasterizeAndRecord(page_measurement.PageMeasurement):
  def __init__(self):
    super(RasterizeAndRecord, self).__init__('', True)
    self._metrics = None

  def AddCommandLineOptions(self, parser):
    parser.add_option('--raster-record-repeat', dest='raster_record_repeat',
                      default=20,
                      help='Repetitions in raster and record loops.' +
                      'Higher values reduce variance, but can cause' +
                      'instability (timeouts, event buffer overflows, etc.).')
    parser.add_option('--start-wait-time', dest='start_wait_time',
                      default=2,
                      help='Wait time before the benchmark is started ' +
                      '(must be long enought to load all content)')
    parser.add_option('--stop-wait-time', dest='stop_wait_time',
                      default=5,
                      help='Wait time before measurement is taken ' +
                      '(must be long enough to render one frame)')

  def CustomizeBrowserOptions(self, options):
    smoothness.SmoothnessMetrics.CustomizeBrowserOptions(options)
    # Run each raster task N times. This allows us to report the time for the
    # best run, effectively excluding cache effects and time when the thread is
    # de-scheduled.
    options.AppendExtraBrowserArgs([
        '--slow-down-raster-scale-factor=%d' % int(
            options.raster_record_repeat),
        # Enable impl-side-painting. Current version of benchmark only works for
        # this mode.
        '--enable-impl-side-painting',
        '--force-compositing-mode',
        '--enable-threaded-compositing'
    ])

  def MeasurePage(self, page, tab, results):
    self._metrics = smoothness.SmoothnessMetrics(tab)

    # Rasterize only what's visible.
    tab.ExecuteJavaScript(
        'chrome.gpuBenchmarking.setRasterizeOnlyVisibleContent();')

    # Wait until the page has loaded and come to a somewhat steady state.
    # Needs to be adjusted for every device (~2 seconds for workstation).
    time.sleep(float(self.options.start_wait_time))

    # Render one frame before we start gathering a trace. On some pages, the
    # first frame requested has more variance in the number of pixels
    # rasterized.
    tab.ExecuteJavaScript("""
        window.__rafFired = false;
        window.webkitRequestAnimationFrame(function() {
          chrome.gpuBenchmarking.setNeedsDisplayOnAllLayers();
          window.__rafFired  = true;
        });
    """)

    time.sleep(float(self.options.stop_wait_time))
    tab.browser.StartTracing('webkit.console,benchmark', 60)
    self._metrics.Start()

    tab.ExecuteJavaScript("""
        window.__rafFired = false;
        window.webkitRequestAnimationFrame(function() {
          chrome.gpuBenchmarking.setNeedsDisplayOnAllLayers();
          console.time("measureNextFrame");
          window.__rafFired  = true;
        });
    """)
    # Wait until the frame was drawn.
    # Needs to be adjusted for every device and for different
    # raster_record_repeat counts.
    # TODO(ernstm): replace by call-back.
    time.sleep(float(self.options.stop_wait_time))
    tab.ExecuteJavaScript('console.timeEnd("measureNextFrame")')

    self._metrics.Stop()
    rendering_stats_deltas = self._metrics.deltas
    timeline = tab.browser.StopTracing().AsTimelineModel()
    timeline_markers = smoothness.FindTimelineMarkers(timeline,
                                                     'measureNextFrame')
    benchmark_stats = RenderingStats(timeline_markers,
                                     timeline_markers,
                                     rendering_stats_deltas,
                                     self._metrics.is_using_gpu_benchmarking)

    results.Add('rasterize_time', 'ms',
                max(benchmark_stats.rasterize_time))
    results.Add('record_time', 'ms',
                max(benchmark_stats.record_time))
    results.Add('rasterized_pixels', 'pixels',
                max(benchmark_stats.rasterized_pixel_count))
    results.Add('recorded_pixels', 'pixels',
                max(benchmark_stats.recorded_pixel_count))
