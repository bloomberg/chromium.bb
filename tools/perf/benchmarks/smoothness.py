# Copyright (c) 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from benchmarks import silk_flags
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
  page_set = 'page_sets/tough_canvas_cases.py'


class SmoothnessToughWebGLCases(test.Test):
  test = smoothness.Smoothness
  page_set = 'page_sets/tough_webgl_cases.py'


class SmoothnessMaps(test.Test):
  test = smoothness.Smoothness
  page_set = 'page_sets/maps.json'


class SmoothnessKeyMobileSites(test.Test):
  """Measures rendering statistics while scrolling down the key mobile sites.

  http://www.chromium.org/developers/design-documents/rendering-benchmarks"""
  test = smoothness.Smoothness
  page_set = 'page_sets/key_mobile_sites.py'


@test.Disabled('android')  # crbug.com/350692
class SmoothnessToughAnimationCases(test.Test):
  test = smoothness.Smoothness
  page_set = 'page_sets/tough_animation_cases.py'


class SmoothnessKeySilkCases(test.Test):
  """Measures rendering statistics for the key silk cases without GPU
  rasterization
  """
  test = smoothness.Smoothness
  page_set = 'page_sets/key_silk_cases.py'


class SmoothnessFastPathKeySilkCases(test.Test):
  """Measures rendering statistics for the key silk cases without GPU
  rasterization using bleeding edge rendering fast paths.
  """
  tag = 'fast_path'
  test = smoothness.Smoothness
  page_set = 'page_sets/key_silk_cases.py'
  def CustomizeBrowserOptions(self, options):
    silk_flags.CustomizeBrowserOptionsForFastPath(options)


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
    options.AppendExtraBrowserArgs('--force-gpu-rasterization')


class SmoothnessGpuRasterizationKeyMobileSites(test.Test):
  """Measures rendering statistics for the key mobile sites with GPU
  rasterization
  """
  tag = 'gpu_rasterization'
  test = smoothness.Smoothness
  page_set = 'page_sets/key_mobile_sites.py'
  def CustomizeBrowserOptions(self, options):
    options.AppendExtraBrowserArgs('--enable-threaded-compositing')
    options.AppendExtraBrowserArgs('--force-compositing-mode')
    options.AppendExtraBrowserArgs('--enable-impl-side-painting')
    options.AppendExtraBrowserArgs('--force-gpu-rasterization')


class SmoothnessGpuRasterizationKeySilkCases(test.Test):
  """Measures rendering statistics for the key silk cases with GPU rasterization
  """
  tag = 'gpu_rasterization'
  test = smoothness.Smoothness
  page_set = 'page_sets/key_silk_cases.py'
  def CustomizeBrowserOptions(self, options):
    options.AppendExtraBrowserArgs('--enable-threaded-compositing')
    options.AppendExtraBrowserArgs('--force-compositing-mode')
    options.AppendExtraBrowserArgs('--enable-impl-side-painting')
    options.AppendExtraBrowserArgs('--force-gpu-rasterization')


class SmoothnessFastPathGpuRasterizationKeySilkCases(
    SmoothnessGpuRasterizationKeySilkCases):
  """Measures rendering statistics for the key silk cases with GPU rasterization
  using bleeding edge rendering fast paths.
  """
  tag = 'fast_path_gpu_rasterization'
  test = smoothness.Smoothness
  page_set = 'page_sets/key_silk_cases.py'
  def CustomizeBrowserOptions(self, options):
    super(SmoothnessFastPathGpuRasterizationKeySilkCases, self). \
        CustomizeBrowserOptions(options)
    silk_flags.CustomizeBrowserOptionsForFastPath(options)


@test.Enabled('android')
class SmoothnessToughPinchZoomCases(test.Test):
  """Measures rendering statistics for pinch-zooming into the tough pinch zoom
  cases
  """
  test = smoothness.Smoothness
  page_set = 'page_sets/tough_pinch_zoom_cases.py'
