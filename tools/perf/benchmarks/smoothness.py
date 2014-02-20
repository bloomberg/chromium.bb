# Copyright (c) 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from measurements import smoothness
from telemetry import test


class SmoothnessTop25(test.Test):
  """Measures rendering statistics while scrolling down the top 25 web pages.

  http://www.chromium.org/developers/design-documents/rendering-benchmarks"""
  test = smoothness.Smoothness
  page_set = 'page_sets/top_25.json'


@test.Disabled('mac')
class SmoothnessToughCanvasCases(test.Test):
  test = smoothness.Smoothness
  page_set = 'page_sets/tough_canvas_cases.json'


class SmoothnessKeyMobileSites(test.Test):
  """Measures rendering statistics while scrolling down the key mobile sites.

  http://www.chromium.org/developers/design-documents/rendering-benchmarks"""
  test = smoothness.Smoothness
  page_set = 'page_sets/key_mobile_sites.json'


class SmoothnessToughAnimationCases(test.Test):
  test = smoothness.Smoothness
  page_set = 'page_sets/tough_animation_cases.json'


class SmoothnessKeySilkCases(test.Test):
  """Measures rendering statistics for the key silk cases without GPU
  rasterization
  """
  test = smoothness.Smoothness
  page_set = 'page_sets/key_silk_cases.json'


class SmoothnessGpuRasterizationTop25(test.Test):
  """Measures rendering statistics for the top 25 with GPU rasterization
  """
  tag = 'gpu_rasterization'
  test = smoothness.Smoothness
  page_set = 'page_sets/top_25.json'
  def CustomizeBrowserOptions(self, options):
    options.AppendExtraBrowserArgs('--enable-threaded-compositing')
    options.AppendExtraBrowserArgs('--force-compositing-mode')
    options.AppendExtraBrowserArgs('--enable-impl-side-painting')
    options.AppendExtraBrowserArgs('--enable-gpu-rasterization')


class SmoothnessGpuRasterizationKeyMobileSites(test.Test):
  """Measures rendering statistics for the key mobile sites with GPU
  rasterization
  """
  tag = 'gpu_rasterization'
  test = smoothness.Smoothness
  page_set = 'page_sets/key_mobile_sites.json'
  def CustomizeBrowserOptions(self, options):
    options.AppendExtraBrowserArgs('--enable-threaded-compositing')
    options.AppendExtraBrowserArgs('--force-compositing-mode')
    options.AppendExtraBrowserArgs('--enable-impl-side-painting')
    options.AppendExtraBrowserArgs('--enable-gpu-rasterization')


class SmoothnessGpuRasterizationKeySilkCases(test.Test):
  """Measures rendering statistics for the key silk cases with GPU rasterization
  """
  tag = 'gpu_rasterization'
  test = smoothness.Smoothness
  page_set = 'page_sets/key_silk_cases.json'
  def CustomizeBrowserOptions(self, options):
    options.AppendExtraBrowserArgs('--enable-threaded-compositing')
    options.AppendExtraBrowserArgs('--force-compositing-mode')
    options.AppendExtraBrowserArgs('--enable-impl-side-painting')
    options.AppendExtraBrowserArgs('--enable-gpu-rasterization')
