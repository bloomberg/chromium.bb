# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from benchmarks import silk_flags
from measurements import repaint
import page_sets
from telemetry import benchmark


class RepaintKeyMobileSites(benchmark.Benchmark):
  """Measures repaint performance on the key mobile sites.

  http://www.chromium.org/developers/design-documents/rendering-benchmarks"""
  test = repaint.Repaint
  page_set = page_sets.KeyMobileSitesPageSet


@benchmark.Disabled('android')  # crbug.com/399125
class RepaintGpuRasterizationKeyMobileSites(benchmark.Benchmark):
  """Measures repaint performance on the key mobile sites with forced GPU
  rasterization.

  http://www.chromium.org/developers/design-documents/rendering-benchmarks"""
  tag = 'gpu_rasterization'
  test = repaint.Repaint
  page_set = page_sets.KeyMobileSitesPageSet
  def CustomizeBrowserOptions(self, options):
    silk_flags.CustomizeBrowserOptionsForGpuRasterization(options)
