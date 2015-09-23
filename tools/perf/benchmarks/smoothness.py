# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from core import perf_benchmark

from benchmarks import silk_flags
from measurements import smoothness
import page_sets
import page_sets.key_silk_cases
from telemetry import benchmark


class SmoothnessTop25(perf_benchmark.PerfBenchmark):
  """Measures rendering statistics while scrolling down the top 25 web pages.

  http://www.chromium.org/developers/design-documents/rendering-benchmarks
  """
  test = smoothness.Smoothness
  page_set = page_sets.Top25SmoothPageSet

  @classmethod
  def Name(cls):
    return 'smoothness.top_25_smooth'


class SmoothnessToughFiltersCases(perf_benchmark.PerfBenchmark):
  """Measures frame rate and a variety of other statistics.

  Uses a selection of pages making use of SVG and CSS Filter Effects.
  """
  test = smoothness.Smoothness
  page_set = page_sets.ToughFiltersCasesPageSet

  @classmethod
  def Name(cls):
    return 'smoothness.tough_filters_cases'


class SmoothnessToughPathRenderingCases(perf_benchmark.PerfBenchmark):
  """Tests a selection of pages with SVG and 2D Canvas paths.

  Measures frame rate and a variety of other statistics.  """
  test = smoothness.Smoothness
  page_set = page_sets.ToughPathRenderingCasesPageSet

  @classmethod
  def Name(cls):
    return 'smoothness.tough_path_rendering_cases'


@benchmark.Disabled('android')  # crbug.com/526901
class SmoothnessToughCanvasCases(perf_benchmark.PerfBenchmark):
  """Measures frame rate and a variety of other statistics.

  Uses a selection of pages making use of the 2D Canvas API.
  """
  test = smoothness.Smoothness
  page_set = page_sets.ToughCanvasCasesPageSet

  @classmethod
  def Name(cls):
    return 'smoothness.tough_canvas_cases'


@benchmark.Disabled('android')  # crbug.com/373812
class SmoothnessToughWebGLCases(perf_benchmark.PerfBenchmark):
  test = smoothness.Smoothness
  page_set = page_sets.ToughWebglCasesPageSet

  @classmethod
  def Name(cls):
    return 'smoothness.tough_webgl_cases'


@benchmark.Enabled('android')
class SmoothnessMaps(perf_benchmark.PerfBenchmark):
  page_set = page_sets.MapsPageSet

  @classmethod
  def Name(cls):
    return 'smoothness.maps'


@benchmark.Disabled('android')
class SmoothnessKeyDesktopMoveCases(perf_benchmark.PerfBenchmark):
  test = smoothness.Smoothness
  page_set = page_sets.KeyDesktopMoveCasesPageSet

  @classmethod
  def Name(cls):
    return 'smoothness.key_desktop_move_cases'


@benchmark.Enabled('android')
class SmoothnessKeyMobileSites(perf_benchmark.PerfBenchmark):
  """Measures rendering statistics while scrolling down the key mobile sites.

  http://www.chromium.org/developers/design-documents/rendering-benchmarks
  """
  test = smoothness.Smoothness
  page_set = page_sets.KeyMobileSitesSmoothPageSet

  @classmethod
  def Name(cls):
    return 'smoothness.key_mobile_sites_smooth'


class SmoothnessToughAnimationCases(perf_benchmark.PerfBenchmark):
  test = smoothness.SmoothnessWithRestart
  page_set = page_sets.ToughAnimationCasesPageSet

  @classmethod
  def Name(cls):
    return 'smoothness.tough_animation_cases'


@benchmark.Enabled('android')
class SmoothnessKeySilkCases(perf_benchmark.PerfBenchmark):
  """Measures rendering statistics for the key silk cases without GPU
  rasterization.
  """
  test = smoothness.Smoothness
  page_set = page_sets.KeySilkCasesPageSet

  @classmethod
  def Name(cls):
    return 'smoothness.key_silk_cases'

  def CreateStorySet(self, options):
    stories = super(SmoothnessKeySilkCases, self).CreateStorySet(options)
    # Page26 (befamous) is too noisy to be useful; crbug.com/461127
    to_remove = [story for story in stories
                 if isinstance(story, page_sets.key_silk_cases.Page26)]
    for story in to_remove:
      stories.RemoveStory(story)
    return stories


@benchmark.Enabled('android')
class SmoothnessGpuRasterizationTop25(perf_benchmark.PerfBenchmark):
  """Measures rendering statistics for the top 25 with GPU rasterization.
  """
  tag = 'gpu_rasterization'
  test = smoothness.Smoothness
  page_set = page_sets.Top25SmoothPageSet

  def SetExtraBrowserOptions(self, options):
    silk_flags.CustomizeBrowserOptionsForGpuRasterization(options)

  @classmethod
  def Name(cls):
    return 'smoothness.gpu_rasterization.top_25_smooth'


@benchmark.Enabled('android')
class SmoothnessGpuRasterizationKeyMobileSites(perf_benchmark.PerfBenchmark):
  """Measures rendering statistics for the key mobile sites with GPU
  rasterization.
  """
  tag = 'gpu_rasterization'
  test = smoothness.Smoothness
  page_set = page_sets.KeyMobileSitesSmoothPageSet

  def SetExtraBrowserOptions(self, options):
    silk_flags.CustomizeBrowserOptionsForGpuRasterization(options)

  @classmethod
  def Name(cls):
    return 'smoothness.gpu_rasterization.key_mobile_sites_smooth'


class SmoothnessGpuRasterizationToughPathRenderingCases(
    perf_benchmark.PerfBenchmark):
  """Tests a selection of pages with SVG and 2D canvas paths with GPU
  rasterization.
  """
  tag = 'gpu_rasterization'
  test = smoothness.Smoothness
  page_set = page_sets.ToughPathRenderingCasesPageSet

  def SetExtraBrowserOptions(self, options):
    silk_flags.CustomizeBrowserOptionsForGpuRasterization(options)

  @classmethod
  def Name(cls):
    return 'smoothness.gpu_rasterization.tough_path_rendering_cases'


class SmoothnessGpuRasterizationFiltersCases(perf_benchmark.PerfBenchmark):
  """Tests a selection of pages with SVG and CSS filter effects with GPU
  rasterization.
  """
  tag = 'gpu_rasterization'
  test = smoothness.Smoothness
  page_set = page_sets.ToughFiltersCasesPageSet

  def SetExtraBrowserOptions(self, options):
    silk_flags.CustomizeBrowserOptionsForGpuRasterization(options)

  @classmethod
  def Name(cls):
    return 'smoothness.gpu_rasterization.tough_filters_cases'


@benchmark.Enabled('android')
class SmoothnessSyncScrollKeyMobileSites(perf_benchmark.PerfBenchmark):
  """Measures rendering statistics for the key mobile sites with synchronous
  (main thread) scrolling.
  """
  tag = 'sync_scroll'
  test = smoothness.Smoothness
  page_set = page_sets.KeyMobileSitesSmoothPageSet

  def SetExtraBrowserOptions(self, options):
    silk_flags.CustomizeBrowserOptionsForSyncScrolling(options)

  @classmethod
  def Name(cls):
    return 'smoothness.sync_scroll.key_mobile_sites_smooth'


@benchmark.Enabled('android')
class SmoothnessSimpleMobilePages(perf_benchmark.PerfBenchmark):
  """Measures rendering statistics for simple mobile sites page set.
  """
  test = smoothness.Smoothness
  page_set = page_sets.SimpleMobileSitesPageSet

  @classmethod
  def Name(cls):
    return 'smoothness.simple_mobile_sites'


@benchmark.Enabled('android')
class SmoothnessFlingSimpleMobilePages(perf_benchmark.PerfBenchmark):
  """Measures rendering statistics for flinging a simple mobile sites page set.
  """
  test = smoothness.Smoothness
  page_set = page_sets.SimpleMobileSitesFlingPageSet

  def SetExtraBrowserOptions(self, options):
    # As the fling parameters cannot be analytically determined to not
    # overscroll, disable overscrolling explicitly. Overscroll behavior is
    # orthogonal to fling performance, and its activation is only more noise.
    options.AppendExtraBrowserArgs('--disable-overscroll-edge-effect')

  @classmethod
  def Name(cls):
    return 'smoothness.fling.simple_mobile_sites'


@benchmark.Enabled('android', 'chromeos', 'mac')
class SmoothnessToughPinchZoomCases(perf_benchmark.PerfBenchmark):
  """Measures rendering statistics for pinch-zooming into the tough pinch zoom
  cases.
  """
  test = smoothness.Smoothness
  page_set = page_sets.ToughPinchZoomCasesPageSet

  @classmethod
  def Name(cls):
    return 'smoothness.tough_pinch_zoom_cases'


@benchmark.Enabled('android', 'chromeos')
class SmoothnessToughScrollingWhileZoomedInCases(perf_benchmark.PerfBenchmark):
  """Measures rendering statistics for pinch-zooming then diagonal scrolling"""
  test = smoothness.Smoothness
  page_set = page_sets.ToughScrollingWhileZoomedInCasesPageSet

  @classmethod
  def Name(cls):
    return 'smoothness.tough_scrolling_while_zoomed_in_cases'


@benchmark.Enabled('android')
class SmoothnessPolymer(perf_benchmark.PerfBenchmark):
  """Measures rendering statistics for Polymer cases.
  """
  test = smoothness.Smoothness
  page_set = page_sets.PolymerPageSet

  @classmethod
  def Name(cls):
    return 'smoothness.polymer'


@benchmark.Enabled('android')
class SmoothnessGpuRasterizationPolymer(perf_benchmark.PerfBenchmark):
  """Measures rendering statistics for the Polymer cases with GPU rasterization.
  """
  tag = 'gpu_rasterization'
  test = smoothness.Smoothness
  page_set = page_sets.PolymerPageSet

  def SetExtraBrowserOptions(self, options):
    silk_flags.CustomizeBrowserOptionsForGpuRasterization(options)

  @classmethod
  def Name(cls):
    return 'smoothness.gpu_rasterization.polymer'


class SmoothnessToughScrollingCases(perf_benchmark.PerfBenchmark):
  test = smoothness.Smoothness
  page_set = page_sets.ToughScrollingCasesPageSet

  @classmethod
  def Name(cls):
    return 'smoothness.tough_scrolling_cases'

@benchmark.Disabled('android')  # http://crbug.com/531593
class SmoothnessToughImageDecodeCases(perf_benchmark.PerfBenchmark):
  test = smoothness.Smoothness
  page_set = page_sets.ToughImageDecodeCasesPageSet

  @classmethod
  def Name(cls):
    return 'smoothness.tough_image_decode_cases'

@benchmark.Disabled('android')  # http://crbug.com/513699
class SmoothnessImageDecodingCases(perf_benchmark.PerfBenchmark):
  """Measures decoding statistics for jpeg images.
  """
  test = smoothness.Smoothness
  page_set = page_sets.ImageDecodingCasesPageSet

  def SetExtraBrowserOptions(self, options):
    silk_flags.CustomizeBrowserOptionsForGpuRasterization(options)
    options.AppendExtraBrowserArgs('--disable-accelerated-jpeg-decoding')

  @classmethod
  def Name(cls):
    return 'smoothness.image_decoding_cases'


@benchmark.Disabled('android')  # http://crbug.com/513699
class SmoothnessGpuImageDecodingCases(perf_benchmark.PerfBenchmark):
  """Measures decoding statistics for jpeg images with GPU rasterization.
  """
  tag = 'gpu_rasterization_and_decoding'
  test = smoothness.Smoothness
  page_set = page_sets.ImageDecodingCasesPageSet

  def SetExtraBrowserOptions(self, options):
    silk_flags.CustomizeBrowserOptionsForGpuRasterization(options)
    # TODO(sugoi): Remove the following line once M41 goes stable
    options.AppendExtraBrowserArgs('--enable-accelerated-jpeg-decoding')

  @classmethod
  def Name(cls):
    return 'smoothness.gpu_rasterization_and_decoding.image_decoding_cases'


@benchmark.Enabled('android')
class SmoothnessPathologicalMobileSites(perf_benchmark.PerfBenchmark):
  """Measures task execution statistics while scrolling pathological sites.
  """
  test = smoothness.Smoothness
  page_set = page_sets.PathologicalMobileSitesPageSet

  @classmethod
  def Name(cls):
    return 'smoothness.pathological_mobile_sites'


class SmoothnessToughAnimatedImageCases(perf_benchmark.PerfBenchmark):
  test = smoothness.Smoothness
  page_set = page_sets.ToughAnimatedImageCasesPageSet

  @classmethod
  def Name(cls):
    return 'smoothness.tough_animated_image_cases'


@benchmark.Disabled('reference')  # http://crbug.com/499489
class SmoothnessToughTextureUploadCases(perf_benchmark.PerfBenchmark):
  test = smoothness.Smoothness
  page_set = page_sets.ToughTextureUploadCasesPageSet

  @classmethod
  def Name(cls):
    return 'smoothness.tough_texture_upload_cases'


@benchmark.Disabled('reference')  # http://crbug.com/496684
class SmoothnessToughAdCases(perf_benchmark.PerfBenchmark):
  """Measures rendering statistics while displaying advertisements."""
  test = smoothness.Smoothness
  page_set = page_sets.ToughAdCasesPageSet

  @classmethod
  def Name(cls):
    return 'smoothness.tough_ad_cases'


# http://crbug.com/496684 (reference)
# http://crbug.com/522619 (mac/win)
@benchmark.Disabled('reference', 'win', 'mac')
class SmoothnessScrollingToughAdCases(perf_benchmark.PerfBenchmark):
  """Measures rendering statistics while scrolling advertisements."""
  test = smoothness.Smoothness
  page_set = page_sets.ScrollingToughAdCasesPageSet

  @classmethod
  def Name(cls):
    return 'smoothness.scrolling_tough_ad_cases'


# http://crbug.com/496684 (reference)
# http://crbug.com/522619 (mac/win)
@benchmark.Disabled('reference', 'win', 'mac')
class SmoothnessBidirectionallyScrollingToughAdCases(
    perf_benchmark.PerfBenchmark):
  """Measures rendering statistics while scrolling advertisements."""
  test = smoothness.Smoothness
  page_set = page_sets.BidirectionallyScrollingToughAdCasesPageSet

  def SetExtraBrowserOptions(self, options):
    # Don't accidentally reload the page while scrolling.
    options.AppendExtraBrowserArgs('--disable-pull-to-refresh-effect')

  @classmethod
  def Name(cls):
    return 'smoothness.bidirectionally_scrolling_tough_ad_cases'


@benchmark.Disabled('reference')  # http://crbug.com/496684
class SmoothnessToughWebGLAdCases(perf_benchmark.PerfBenchmark):
  """Measures rendering statistics while scrolling advertisements."""
  test = smoothness.Smoothness
  page_set = page_sets.ToughWebglAdCasesPageSet

  @classmethod
  def Name(cls):
    return 'smoothness.tough_webgl_ad_cases'
