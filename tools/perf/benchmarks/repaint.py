# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from measurements import repaint
from telemetry import test


class RepaintKeyMobileSites(test.Test):
  """Measures repaint performance on the key mobile sites.

  http://www.chromium.org/developers/design-documents/rendering-benchmarks"""
  test = repaint.Repaint
  page_set = 'page_sets/key_mobile_sites.json'


class RepaintGpuRasterizationKeyMobileSites(test.Test):
  """Measures repaint performance on the key mobile sites with forced GPU
  rasterization.

  http://www.chromium.org/developers/design-documents/rendering-benchmarks"""
  tag = 'gpu_rasterization'
  test = repaint.Repaint
  page_set = 'page_sets/key_mobile_sites.json'
  def CustomizeBrowserOptions(self, options):
    options.AppendExtraBrowserArgs('--enable-threaded-compositing')
    options.AppendExtraBrowserArgs('--force-compositing-mode')
    options.AppendExtraBrowserArgs('--enable-impl-side-painting')
    options.AppendExtraBrowserArgs('--force-gpu-rasterization')
