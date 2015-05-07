# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from telemetry import benchmark

from benchmarks import silk_flags
from benchmarks import webgl_expectations
from measurements import smoothness
import page_sets


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


class SmoothnessToughPathRenderingCases(benchmark.Benchmark):
  """Tests a selection of pages with SVG and 2D Canvas paths.

  Measures frame rate and a variety of other statistics.  """
  test = smoothness.Smoothness
  page_set = page_sets.ToughPathRenderingCasesPageSet

  @classmethod
  def Name(cls):
    return 'smoothness.tough_path_rendering_cases'


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


@benchmark.Disabled('android')  # crbug.com/373812
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
  page_set = page_sets.MapsPageSet

  @classmethod
  def CreateExpectations(cls):
    return webgl_expectations.MapsExpectations()

  @classmethod
  def Name(cls):
    return 'smoothness.maps'


@benchmark.Disabled('android')
class SmoothnessKeyDesktopMoveCases(benchmark.Benchmark):
  test = smoothness.Smoothness
  page_set = page_sets.KeyDesktopMoveCasesPageSet

  @classmethod
  def Name(cls):
    return 'smoothness.key_desktop_move_cases'


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


@benchmark.Disabled('mac', 'win', 'android')
class SmoothnessKeyMobileSitesWithSlimmingPaint(benchmark.Benchmark):
  """Measures smoothness on key mobile sites with --enable-slimming-paint.

  http://www.chromium.org/developers/design-documents/rendering-benchmarks
  """
  test = smoothness.Smoothness
  page_set = page_sets.KeyMobileSitesSmoothPageSet

  def CustomizeBrowserOptions(self, options):
    options.AppendExtraBrowserArgs(['--enable-slimming-paint'])

  @classmethod
  def Name(cls):
    return 'smoothness.key_mobile_sites_with_slimming_paint_smooth'


class SmoothnessToughAnimationCases(benchmark.Benchmark):
  test = smoothness.SmoothnessWithRestart
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


class SmoothnessGpuRasterizationToughPathRenderingCases(benchmark.Benchmark):
  """Tests a selection of pages with SVG and 2D canvas paths with GPU
  rasterization.
  """
  tag = 'gpu_rasterization'
  test = smoothness.Smoothness
  page_set = page_sets.ToughPathRenderingCasesPageSet

  def CustomizeBrowserOptions(self, options):
    silk_flags.CustomizeBrowserOptionsForGpuRasterization(options)

  @classmethod
  def Name(cls):
    return 'smoothness.gpu_rasterization.tough_path_rendering_cases'


class SmoothnessGpuRasterizationFiltersCases(benchmark.Benchmark):
  """Tests a selection of pages with SVG and CSS filter effects with GPU
  rasterization.
  """
  tag = 'gpu_rasterization'
  test = smoothness.Smoothness
  page_set = page_sets.ToughFiltersCasesPageSet

  def CustomizeBrowserOptions(self, options):
    silk_flags.CustomizeBrowserOptionsForGpuRasterization(options)

  @classmethod
  def Name(cls):
    return 'smoothness.gpu_rasterization.tough_filters_cases'


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


@benchmark.Enabled('android')
class SmoothnessFlingSimpleMobilePages(benchmark.Benchmark):
  """Measures rendering statistics for flinging a simple mobile sites page set.
  """
  test = smoothness.Smoothness
  page_set = page_sets.SimpleMobileSitesFlingPageSet

  def CustomizeBrowserOptions(self, options):
    # As the fling parameters cannot be analytically determined to not
    # overscroll, disable overscrolling explicitly. Overscroll behavior is
    # orthogonal to fling performance, and its activation is only more noise.
    options.AppendExtraBrowserArgs('--disable-overscroll-edge-effect')

  @classmethod
  def Name(cls):
    return 'smoothness.fling.simple_mobile_sites'


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


@benchmark.Enabled('android', 'chromeos')
class SmoothnessToughScrollingWhileZoomedInCases(benchmark.Benchmark):
  """Measures rendering statistics for pinch-zooming then diagonal scrolling"""
  test = smoothness.Smoothness
  page_set = page_sets.ToughScrollingWhileZoomedInCasesPageSet

  @classmethod
  def Name(cls):
    return 'smoothness.tough_scrolling_while_zoomed_in_cases'


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


class SmoothnessToughScrollingCases(benchmark.Benchmark):
  test = smoothness.Smoothness
  page_set = page_sets.ToughScrollingCasesPageSet

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


@benchmark.Enabled('android')
class SmoothnessSyncScrollPathologicalMobileSites(benchmark.Benchmark):
  """Measures task execution statistics while sync-scrolling pathological sites.
  """
  tag = 'sync_scroll'
  page_set = page_sets.PathologicalMobileSitesPageSet
  test = smoothness.Smoothness

  def CustomizeBrowserOptions(self, options):
    silk_flags.CustomizeBrowserOptionsForSyncScrolling(options)

  @classmethod
  def Name(cls):
    return 'smoothness.sync_scroll.pathological_mobile_sites'


class SmoothnessToughAnimatedImageCases(benchmark.Benchmark):
  test = smoothness.Smoothness
  page_set = page_sets.ToughAnimatedImageCasesPageSet

  @classmethod
  def Name(cls):
    return 'smoothness.tough_animated_image_cases'
