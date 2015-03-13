# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from benchmarks import silk_flags
from benchmarks import webgl_expectations
from measurements import smoothness
import page_sets
from telemetry import benchmark


class SmoothnessTop25(benchmark.Benchmark):
  """Measures rendering statistics while scrolling down the top 25 web pages.

  http://www.chromium.org/developers/design-documents/rendering-benchmarks
  """
  test = smoothness.Smoothness
  page_set = page_sets.Top25SmoothPageSet

  @classmethod
  def Name(cls):
    return 'smoothness.top_25_smooth'


class SmoothnessToughFiltersCases(benchmark.Benchmark):
  """Measures frame rate and a variety of other statistics.

  Uses a selection of pages making use of SVG and CSS Filter Effects.
  """
  test = smoothness.Smoothness
  page_set = page_sets.ToughFiltersCasesPageSet

  @classmethod
  def Name(cls):
    return 'smoothness.tough_filters_cases'


# crbug.com/388877, crbug.com/396127
@benchmark.Disabled('mac', 'win', 'android')
class SmoothnessToughCanvasCases(benchmark.Benchmark):
  """Measures frame rate and a variety of other statistics.

  Uses a selection of pages making use of the 2D Canvas API.
  """
  test = smoothness.Smoothness
  page_set = page_sets.ToughCanvasCasesPageSet

  @classmethod
  def Name(cls):
    return 'smoothness.tough_canvas_cases'


@benchmark.Disabled('android', 'mac', 'win')  # crbug.com/373812
class SmoothnessToughWebGLCases(benchmark.Benchmark):
  test = smoothness.Smoothness
  page_set = page_sets.ToughWebglCasesPageSet

  @classmethod
  def CreateExpectations(cls):
    return webgl_expectations.WebGLExpectations()

  @classmethod
  def Name(cls):
    return 'smoothness.tough_webgl_cases'


@benchmark.Enabled('android')
class SmoothnessMaps(benchmark.Benchmark):
  test = smoothness.Smoothness
  page_set = page_sets.MapsPageSet

  @classmethod
  def Name(cls):
    return 'smoothness.maps'


@benchmark.Enabled('android')
class SmoothnessKeyMobileSites(benchmark.Benchmark):
  """Measures rendering statistics while scrolling down the key mobile sites.

  http://www.chromium.org/developers/design-documents/rendering-benchmarks
  """
  test = smoothness.Smoothness
  page_set = page_sets.KeyMobileSitesSmoothPageSet

  @classmethod
  def Name(cls):
    return 'smoothness.key_mobile_sites_smooth'


class SmoothnessToughAnimationCases(benchmark.Benchmark):
  test = smoothness.Smoothness
  page_set = page_sets.ToughAnimationCasesPageSet

  @classmethod
  def Name(cls):
    return 'smoothness.tough_animation_cases'


@benchmark.Enabled('android')
class SmoothnessKeySilkCases(benchmark.Benchmark):
  """Measures rendering statistics for the key silk cases without GPU
  rasterization.
  """
  test = smoothness.Smoothness
  page_set = page_sets.KeySilkCasesPageSet

  @classmethod
  def Name(cls):
    return 'smoothness.key_silk_cases'


@benchmark.Enabled('android')
class SmoothnessGpuRasterizationTop25(benchmark.Benchmark):
  """Measures rendering statistics for the top 25 with GPU rasterization.
  """
  tag = 'gpu_rasterization'
  test = smoothness.Smoothness
  page_set = page_sets.Top25SmoothPageSet

  def CustomizeBrowserOptions(self, options):
    silk_flags.CustomizeBrowserOptionsForGpuRasterization(options)

  @classmethod
  def Name(cls):
    return 'smoothness.gpu_rasterization.top_25_smooth'


@benchmark.Enabled('android')
class SmoothnessGpuRasterizationKeyMobileSites(benchmark.Benchmark):
  """Measures rendering statistics for the key mobile sites with GPU
  rasterization.
  """
  tag = 'gpu_rasterization'
  test = smoothness.Smoothness
  page_set = page_sets.KeyMobileSitesSmoothPageSet

  def CustomizeBrowserOptions(self, options):
    silk_flags.CustomizeBrowserOptionsForGpuRasterization(options)

  @classmethod
  def Name(cls):
    return 'smoothness.gpu_rasterization.key_mobile_sites_smooth'


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

  @classmethod
  def Name(cls):
    return 'smoothness.sync_scroll.key_mobile_sites_smooth'


@benchmark.Enabled('android')
class SmoothnessSimpleMobilePages(benchmark.Benchmark):
  """Measures rendering statistics for simple mobile sites page set.
  """
  test = smoothness.Smoothness
  page_set = page_sets.SimpleMobileSitesPageSet

  @classmethod
  def Name(cls):
    return 'smoothness.simple_mobile_sites'


@benchmark.Enabled('android', 'chromeos')
class SmoothnessToughPinchZoomCases(benchmark.Benchmark):
  """Measures rendering statistics for pinch-zooming into the tough pinch zoom
  cases.
  """
  test = smoothness.Smoothness
  page_set = page_sets.ToughPinchZoomCasesPageSet

  @classmethod
  def Name(cls):
    return 'smoothness.tough_pinch_zoom_cases'


@benchmark.Enabled('android')
class SmoothnessPolymer(benchmark.Benchmark):
  """Measures rendering statistics for Polymer cases.
  """
  test = smoothness.Smoothness
  page_set = page_sets.PolymerPageSet

  @classmethod
  def Name(cls):
    return 'smoothness.polymer'


@benchmark.Enabled('android')
class SmoothnessGpuRasterizationPolymer(benchmark.Benchmark):
  """Measures rendering statistics for the Polymer cases with GPU rasterization.
  """
  tag = 'gpu_rasterization'
  test = smoothness.Smoothness
  page_set = page_sets.PolymerPageSet

  def CustomizeBrowserOptions(self, options):
    silk_flags.CustomizeBrowserOptionsForGpuRasterization(options)

  @classmethod
  def Name(cls):
    return 'smoothness.gpu_rasterization.polymer'


class SmoothnessToughFastScrollingCases(benchmark.Benchmark):
  test = smoothness.Smoothness
  page_set = page_sets.ToughScrollingCasesPageSet
  options = {'page_label_filter': 'fastscrolling'}

  @classmethod
  def Name(cls):
    return 'smoothness.tough_scrolling_cases'


class SmoothnessImageDecodingCases(benchmark.Benchmark):
  """Measures decoding statistics for jpeg images.
  """
  test = smoothness.Smoothness
  page_set = page_sets.ImageDecodingCasesPageSet

  def CustomizeBrowserOptions(self, options):
    silk_flags.CustomizeBrowserOptionsForGpuRasterization(options)
    options.AppendExtraBrowserArgs('--disable-accelerated-jpeg-decoding')

  @classmethod
  def Name(cls):
    return 'smoothness.image_decoding_cases'


class SmoothnessGpuImageDecodingCases(benchmark.Benchmark):
  """Measures decoding statistics for jpeg images with GPU rasterization.
  """
  tag = 'gpu_rasterization_and_decoding'
  test = smoothness.Smoothness
  page_set = page_sets.ImageDecodingCasesPageSet

  def CustomizeBrowserOptions(self, options):
    silk_flags.CustomizeBrowserOptionsForGpuRasterization(options)
    # TODO(sugoi): Remove the following line once M41 goes stable
    options.AppendExtraBrowserArgs('--enable-accelerated-jpeg-decoding')

  @classmethod
  def Name(cls):
    return 'smoothness.gpu_rasterization_and_decoding.image_decoding_cases'


@benchmark.Enabled('android')
class SmoothnessPathologicalMobileSites(benchmark.Benchmark):
  """Measures task execution statistics while scrolling pathological sites.
  """
  test = smoothness.Smoothness
  page_set = page_sets.PathologicalMobileSitesPageSet

  @classmethod
  def Name(cls):
    return 'smoothness.pathological_mobile_sites'
