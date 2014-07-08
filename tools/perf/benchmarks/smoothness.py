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
  page_set = page_sets.Top25PageSet


@benchmark.Disabled('mac')  # crbug.com/388877
class SmoothnessToughCanvasCases(benchmark.Benchmark):
  test = smoothness.Smoothness
  page_set = page_sets.ToughCanvasCasesPageSet


@benchmark.Disabled('android')  # crbug.com/373812
class SmoothnessToughWebGLCases(benchmark.Benchmark):
  test = smoothness.Smoothness
  page_set = page_sets.ToughWebglCasesPageSet


class SmoothnessMaps(benchmark.Benchmark):
  test = smoothness.Smoothness
  page_set = page_sets.MapsPageSet


class SmoothnessKeyMobileSites(benchmark.Benchmark):
  """Measures rendering statistics while scrolling down the key mobile sites.

  http://www.chromium.org/developers/design-documents/rendering-benchmarks"""
  test = smoothness.Smoothness
  page_set = page_sets.KeyMobileSitesPageSet


@benchmark.Disabled('android')  # crbug.com/350692
class SmoothnessToughAnimationCases(benchmark.Benchmark):
  test = smoothness.Smoothness
  page_set = page_sets.ToughAnimationCasesPageSet


class SmoothnessKeySilkCases(benchmark.Benchmark):
  """Measures rendering statistics for the key silk cases without GPU
  rasterization
  """
  test = smoothness.Smoothness
  page_set = page_sets.KeySilkCasesPageSet


class SmoothnessFastPathKeySilkCases(benchmark.Benchmark):
  """Measures rendering statistics for the key silk cases without GPU
  rasterization using bleeding edge rendering fast paths.
  """
  tag = 'fast_path'
  test = smoothness.Smoothness
  page_set = page_sets.KeySilkCasesPageSet
  def CustomizeBrowserOptions(self, options):
    silk_flags.CustomizeBrowserOptionsForFastPath(options)


@benchmark.Disabled('j')  # crbug.com/363783
class SmoothnessGpuRasterizationTop25(benchmark.Benchmark):
  """Measures rendering statistics for the top 25 with GPU rasterization
  """
  tag = 'gpu_rasterization'
  test = smoothness.Smoothness
  page_set = page_sets.Top25PageSet
  def CustomizeBrowserOptions(self, options):
    silk_flags.CustomizeBrowserOptionsForGpuRasterization(options)


@benchmark.Disabled('j')  # crbug.com/363783
class SmoothnessGpuRasterizationKeyMobileSites(benchmark.Benchmark):
  """Measures rendering statistics for the key mobile sites with GPU
  rasterization
  """
  tag = 'gpu_rasterization'
  test = smoothness.Smoothness
  page_set = page_sets.KeyMobileSitesPageSet
  def CustomizeBrowserOptions(self, options):
    silk_flags.CustomizeBrowserOptionsForGpuRasterization(options)


class SmoothnessGpuRasterizationKeySilkCases(benchmark.Benchmark):
  """Measures rendering statistics for the key silk cases with GPU rasterization
  """
  tag = 'gpu_rasterization'
  test = smoothness.Smoothness
  page_set = page_sets.KeySilkCasesPageSet
  def CustomizeBrowserOptions(self, options):
    silk_flags.CustomizeBrowserOptionsForGpuRasterization(options)


class SmoothnessFastPathGpuRasterizationKeySilkCases(
    SmoothnessGpuRasterizationKeySilkCases):
  """Measures rendering statistics for the key silk cases with GPU rasterization
  using bleeding edge rendering fast paths.
  """
  tag = 'fast_path_gpu_rasterization'
  test = smoothness.Smoothness
  page_set = page_sets.KeySilkCasesPageSet
  def CustomizeBrowserOptions(self, options):
    super(SmoothnessFastPathGpuRasterizationKeySilkCases, self). \
        CustomizeBrowserOptions(options)
    silk_flags.CustomizeBrowserOptionsForFastPath(options)


@benchmark.Enabled('android')
class SmoothnessToughPinchZoomCases(benchmark.Benchmark):
  """Measures rendering statistics for pinch-zooming into the tough pinch zoom
  cases
  """
  test = smoothness.Smoothness
  page_set = page_sets.ToughPinchZoomCasesPageSet


@benchmark.Disabled  # crbug.com/370725
class SmoothnessPolymer(benchmark.Benchmark):
  """Measures rendering statistics for Polymer cases.
  """
  test = smoothness.Smoothness
  page_set = page_sets.PolymerPageSet


@benchmark.Disabled  # crbug.com/370725
class SmoothnessFastPathPolymer(benchmark.Benchmark):
  """Measures rendering statistics for the Polymer cases without GPU
  rasterization using bleeding edge rendering fast paths.
  """
  tag = 'fast_path'
  test = smoothness.Smoothness
  page_set = page_sets.PolymerPageSet
  def CustomizeBrowserOptions(self, options):
    silk_flags.CustomizeBrowserOptionsForFastPath(options)


@benchmark.Disabled  # crbug.com/370725
class SmoothnessGpuRasterizationPolymer(benchmark.Benchmark):
  """Measures rendering statistics for the Polymer cases with GPU rasterization
  """
  tag = 'gpu_rasterization'
  test = smoothness.Smoothness
  page_set = page_sets.PolymerPageSet
  def CustomizeBrowserOptions(self, options):
    silk_flags.CustomizeBrowserOptionsForGpuRasterization(options)


@benchmark.Disabled  # crbug.com/370725
class SmoothnessFastPathGpuRasterizationPolymer(
    SmoothnessGpuRasterizationPolymer):
  """Measures rendering statistics for the Polymer cases with GPU rasterization
  using bleeding edge rendering fast paths.
  """
  tag = 'fast_path_gpu_rasterization'
  test = smoothness.Smoothness
  page_set = page_sets.PolymerPageSet
  def CustomizeBrowserOptions(self, options):
    super(SmoothnessFastPathGpuRasterizationPolymer, self). \
        CustomizeBrowserOptions(options)
    silk_flags.CustomizeBrowserOptionsForFastPath(options)
