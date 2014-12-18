# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from benchmarks import silk_flags
import page_sets
from measurements import smoothness
from telemetry import benchmark


class SmoothnessTop25(benchmark.Benchmark):
  """Measures rendering statistics while scrolling down the top 25 web pages.

  http://www.chromium.org/developers/design-documents/rendering-benchmarks"""
  test = smoothness.Smoothness
  page_set = page_sets.Top25SmoothPageSet


class SmoothnessToughFiltersCases(benchmark.Benchmark):
  test = smoothness.Smoothness
  page_set = page_sets.ToughFiltersCasesPageSet


# crbug.com/388877, crbug.com/396127
@benchmark.Disabled('mac', 'win', 'android')
class SmoothnessToughCanvasCases(benchmark.Benchmark):
  test = smoothness.Smoothness
  page_set = page_sets.ToughCanvasCasesPageSet


@benchmark.Disabled('android', 'mac', 'win')  # crbug.com/373812
class SmoothnessToughWebGLCases(benchmark.Benchmark):
  test = smoothness.Smoothness
  page_set = page_sets.ToughWebglCasesPageSet


@benchmark.Enabled('android')
class SmoothnessMaps(benchmark.Benchmark):
  test = smoothness.Smoothness
  page_set = page_sets.MapsPageSet


@benchmark.Enabled('android')
class SmoothnessKeyMobileSites(benchmark.Benchmark):
  """Measures rendering statistics while scrolling down the key mobile sites.

  http://www.chromium.org/developers/design-documents/rendering-benchmarks"""
  test = smoothness.Smoothness
  page_set = page_sets.KeyMobileSitesSmoothPageSet


class SmoothnessToughAnimationCases(benchmark.Benchmark):
  test = smoothness.Smoothness
  page_set = page_sets.ToughAnimationCasesPageSet


@benchmark.Enabled('android')
class SmoothnessKeySilkCases(benchmark.Benchmark):
  """Measures rendering statistics for the key silk cases without GPU
  rasterization
  """
  test = smoothness.Smoothness
  page_set = page_sets.KeySilkCasesPageSet


@benchmark.Enabled('android')
class SmoothnessGpuRasterizationTop25(benchmark.Benchmark):
  """Measures rendering statistics for the top 25 with GPU rasterization
  """
  tag = 'gpu_rasterization'
  test = smoothness.Smoothness
  page_set = page_sets.Top25SmoothPageSet
  def CustomizeBrowserOptions(self, options):
    silk_flags.CustomizeBrowserOptionsForGpuRasterization(options)


@benchmark.Enabled('android')
class SmoothnessGpuRasterizationKeyMobileSites(benchmark.Benchmark):
  """Measures rendering statistics for the key mobile sites with GPU
  rasterization
  """
  tag = 'gpu_rasterization'
  test = smoothness.Smoothness
  page_set = page_sets.KeyMobileSitesSmoothPageSet
  def CustomizeBrowserOptions(self, options):
    silk_flags.CustomizeBrowserOptionsForGpuRasterization(options)


@benchmark.Enabled('android')
class SmoothnessSyncScrollKeyMobileSites(benchmark.Benchmark):
  """Measures rendering statistics for the key mobile sites with synchronous
  (main thread) scrolling.
  """
  tag = 'sync_scroll'
  test = smoothness.Smoothness
  page_set = page_sets.KeyMobileSitesSmoothPageSet
  def CustomizeBrowserOptions(self, options):
    silk_flags.CustomizeBrowserOptionsForSyncScrolling(options)

@benchmark.Enabled('android')
class SmoothnessSimpleMobilePages(benchmark.Benchmark):
  """Measures rendering statistics for simple mobile sites page set """
  test = smoothness.Smoothness
  page_set = page_sets.SimpleMobileSitesPageSet

@benchmark.Enabled('android')
class SmoothnessToughPinchZoomCases(benchmark.Benchmark):
  """Measures rendering statistics for pinch-zooming into the tough pinch zoom
  cases
  """
  test = smoothness.Smoothness
  page_set = page_sets.ToughPinchZoomCasesPageSet


@benchmark.Enabled('android')
class SmoothnessPolymer(benchmark.Benchmark):
  """Measures rendering statistics for Polymer cases.
  """
  test = smoothness.Smoothness
  page_set = page_sets.PolymerPageSet


@benchmark.Enabled('android')
class SmoothnessGpuRasterizationPolymer(benchmark.Benchmark):
  """Measures rendering statistics for the Polymer cases with GPU rasterization
  """
  tag = 'gpu_rasterization'
  test = smoothness.Smoothness
  page_set = page_sets.PolymerPageSet
  def CustomizeBrowserOptions(self, options):
    silk_flags.CustomizeBrowserOptionsForGpuRasterization(options)


class SmoothnessToughFastScrollingCases(benchmark.Benchmark):
  test = smoothness.Smoothness
  page_set = page_sets.ToughScrollingCasesPageSet
  options = {'page_label_filter' : 'fastscrolling'}


class SmoothnessImageDecodingCases(benchmark.Benchmark):
  """Measures decoding statistics for jpeg images.
  """
  test = smoothness.Smoothness
  page_set = page_sets.ImageDecodingCasesPageSet
  def CustomizeBrowserOptions(self, options):
    silk_flags.CustomizeBrowserOptionsForGpuRasterization(options)
    options.AppendExtraBrowserArgs('--disable-accelerated-jpeg-decoding')


class SmoothnessGpuImageDecodingCases(benchmark.Benchmark):
  """Measures decoding statistics for jpeg images with GPU rasterization
  """
  tag = 'gpu_rasterization_and_decoding'
  test = smoothness.Smoothness
  page_set = page_sets.ImageDecodingCasesPageSet
  def CustomizeBrowserOptions(self, options):
    silk_flags.CustomizeBrowserOptionsForGpuRasterization(options)
    # TODO: Remove the following line once M41 goes stable
    options.AppendExtraBrowserArgs('--enable-accelerated-jpeg-decoding')

