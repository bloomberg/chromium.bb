# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
from telemetry import multi_page_benchmark
from perf_tools import scrolling_benchmark

class PaintingBenchmark(scrolling_benchmark.ScrollingBenchmark):
  def __init__(self):
    super(PaintingBenchmark, self).__init__()

  def _GetOrZero(self, stat, rendering_stats_deltas):
    if stat in rendering_stats_deltas:
      return rendering_stats_deltas[stat]
    return 0

  def _DivideIfPossibleOrZero(self, numerator, denominator):
    if denominator == 0:
      return 0
    return numerator / denominator

  def MeasurePage(self, page, tab, results):
    rendering_stats_deltas = self.ScrollPageFully(page, tab)

    totalPaintTime = self._GetOrZero('totalPaintTimeInSeconds',
                                     rendering_stats_deltas)

    totalRasterizeTime = self._GetOrZero('totalRasterizeTimeInSeconds',
                                         rendering_stats_deltas)

    totalPixelsPainted = self._GetOrZero('totalPixelsPainted',
                                         rendering_stats_deltas)

    totalPixelsRasterized = self._GetOrZero('totalPixelsRasterized',
                                            rendering_stats_deltas)


    megapixelsPaintedPerSecond = self._DivideIfPossibleOrZero(
        (totalPixelsPainted / 1000000.0), totalPaintTime)

    megapixelsRasterizedPerSecond = self._DivideIfPossibleOrZero(
        (totalPixelsRasterized / 1000000.0), totalRasterizeTime)

    results.Add('total_paint_time', 'seconds', totalPaintTime)
    results.Add('total_rasterize_time', 'seconds', totalRasterizeTime)
    results.Add('total_pixels_painted', '', totalPixelsPainted)
    results.Add('total_pixels_rasterized', '', totalPixelsRasterized)
    results.Add('megapixels_painted_per_second', '', megapixelsPaintedPerSecond)
    results.Add('megapixels_rasterized_per_second', '',
                megapixelsRasterizedPerSecond)
    results.Add('total_paint_and_rasterize_time', 'seconds', totalPaintTime +
                totalRasterizeTime)

def Main():
  return multi_page_benchmark.Main(PaintingBenchmark())
