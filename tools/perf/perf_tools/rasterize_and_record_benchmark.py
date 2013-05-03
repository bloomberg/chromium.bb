# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import time

from perf_tools import smoothness_metrics
from telemetry.page import page_measurement

def DivideIfPossibleOrZero(numerator, denominator):
  if denominator == 0:
    return 0
  return numerator / denominator

def CalcPaintingResults(rendering_stats_deltas, results):
  totalPaintTime = rendering_stats_deltas.get('totalPaintTimeInSeconds', 0)
  totalRecordTime = rendering_stats_deltas.get('totalRecordTimeInSeconds', 0)
  # Total rasterize time for all slow-down-raster-scale-factor runs
  totalRasterizeTime = rendering_stats_deltas.get(
    'totalRasterizeTimeInSeconds', 0)
  # Fastest rasterize time out of slow-down-raster-scale-factor runs.
  # This measurement has lower variance than totalRasterizeTime, because it
  # effectively excludes cache effects and time when the thread is de-scheduled.
  bestRasterizeTime = rendering_stats_deltas.get(
    'bestRasterizeTimeInSeconds', 0)
  totalPixelsPainted = rendering_stats_deltas.get('totalPixelsPainted', 0)
  totalPixelsRecorded = rendering_stats_deltas.get('totalPixelsRecorded', 0)
  totalPixelsRasterized = rendering_stats_deltas.get('totalPixelsRasterized', 0)

  megapixelsRecordedPerSecond = DivideIfPossibleOrZero(
      (totalPixelsRecorded / 1000000.0), totalRecordTime)

  megapixelsRasterizedPerSecond = DivideIfPossibleOrZero(
      (totalPixelsRasterized / 1000000.0), totalRasterizeTime)

  results.Add('total_paint_time', 'seconds', totalPaintTime)
  results.Add('total_record_time', 'seconds', totalRecordTime)
  results.Add('total_rasterize_time', 'seconds', totalRasterizeTime,
              data_type='unimportant')
  results.Add('best_rasterize_time', 'seconds', bestRasterizeTime,
              data_type='unimportant')
  results.Add('total_pixels_painted', '', totalPixelsPainted,
              data_type='unimportant')
  results.Add('total_pixels_recorded', '', totalPixelsRecorded,
              data_type='unimportant')
  results.Add('total_pixels_rasterized', '', totalPixelsRasterized,
              data_type='unimportant')
  results.Add('megapixels_recorded_per_second', '', megapixelsRecordedPerSecond,
              data_type='unimportant')
  results.Add('megapixels_rasterized_per_second', '',
              megapixelsRasterizedPerSecond, data_type='unimportant')
  results.Add('total_record_and_rasterize_time', 'seconds', totalRecordTime +
              totalRasterizeTime, data_type='unimportant')

class RasterizeAndPaintMeasurement(page_measurement.PageMeasurement):
  def __init__(self):
    super(RasterizeAndPaintMeasurement, self).__init__('', True)
    self._metrics = None

  def AddCommandLineOptions(self, parser):
    parser.add_option('--report-all-results', dest='report_all_results',
                      action='store_true',
                      help='Reports all data collected')

  def CustomizeBrowserOptions(self, options):
    options.extra_browser_args.append('--enable-gpu-benchmarking')
    # Run each raster task 100 times. This allows us to report the time for the
    # best run, effectively excluding cache effects and time when the thread is
    # de-scheduled.
    options.extra_browser_args.append('--slow-down-raster-scale-factor=100')

  def MeasurePage(self, page, tab, results):
    self._metrics = smoothness_metrics.SmoothnessMetrics(tab)

    # Wait until the page has loaded and come to a somewhat steady state
    # (empirical wait time)
    time.sleep(5)

    self._metrics.SetNeedsDisplayOnAllLayersAndStart()

    # Wait until all rasterization tasks are completed  (empirical wait time)
    # TODO(ernstm): Replace by a more accurate mechanism to measure stats for
    # exactly one frame.
    time.sleep(5)

    self._metrics.Stop()

    rendering_stats_deltas = self._metrics.deltas

    CalcPaintingResults(rendering_stats_deltas, results)

    if self.options.report_all_results:
      for k, v in rendering_stats_deltas.iteritems():
        results.Add(k, '', v)
