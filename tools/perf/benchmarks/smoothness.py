# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
from core import perf_benchmark

from benchmarks import silk_flags
from measurements import smoothness
import page_sets
import page_sets.key_silk_cases
from telemetry import benchmark
from telemetry import story as story_module


class _Smoothness(perf_benchmark.PerfBenchmark):
  """Base class for smoothness-based benchmarks."""

  # Certain smoothness pages do not perform gesture scrolling, in turn yielding
  # an empty first_gesture_scroll_update_latency result. Such empty results
  # should be ignored, allowing aggregate metrics for that page set.
  _PAGES_WITHOUT_SCROLL_GESTURE_BLACKLIST = [
      'http://mobile-news.sandbox.google.com/news/pt0']

  test = smoothness.Smoothness

  @classmethod
  def Name(cls):
    return 'smoothness'

  @classmethod
  def ValueCanBeAddedPredicate(cls, value, is_first_result):
    del is_first_result  # unused
    if (value.name == 'first_gesture_scroll_update_latency' and
        value.page.url in cls._PAGES_WITHOUT_SCROLL_GESTURE_BLACKLIST and
        value.values is None):
      return False
    return True


@benchmark.Owner(emails=['vmiura@chromium.org'])
class SmoothnessTop25(_Smoothness):
  """Measures rendering statistics while scrolling down the top 25 web pages.

  http://www.chromium.org/developers/design-documents/rendering-benchmarks
  """
  page_set = page_sets.Top25SmoothPageSet

  @classmethod
  def Name(cls):
    return 'smoothness.top_25_smooth'

  def GetExpectations(self):
    class StoryExpectations(story_module.expectations.StoryExpectations):
      def SetExpectations(self):
        self.DisableStory('http://www.cnn.com', [story_module.expectations.ALL],
                          'crbug.com/528474')
        self.DisableStory('http://www.amazon.com',
                          [story_module.expectations.ALL],
                          'crbug.com/667432')
        self.DisableStory('https://mail.google.com/mail/',
                          [story_module.expectations.ALL_WIN],
                          'crbug.com/750147')
    return StoryExpectations()


@benchmark.Owner(emails=['senorblanco@chromium.org'])
class SmoothnessToughFiltersCases(_Smoothness):
  """Measures frame rate and a variety of other statistics.

  Uses a selection of pages making use of SVG and CSS Filter Effects.
  """
  page_set = page_sets.ToughFiltersCasesPageSet

  @classmethod
  def Name(cls):
    return 'smoothness.tough_filters_cases'

  def GetExpectations(self):
    class StoryExpectations(story_module.expectations.StoryExpectations):
      def SetExpectations(self):
        self.DisableBenchmark([story_module.expectations.ANDROID_NEXUS6],
                              'crbug.com/624032')
    return StoryExpectations()


@benchmark.Owner(emails=['senorblanco@chromium.org'])
class SmoothnessToughPathRenderingCases(_Smoothness):
  """Tests a selection of pages with SVG and 2D Canvas paths.

  Measures frame rate and a variety of other statistics.  """
  page_set = page_sets.ToughPathRenderingCasesPageSet

  @classmethod
  def Name(cls):
    return 'smoothness.tough_path_rendering_cases'

  def GetExpectations(self):
    class StoryExpectations(story_module.expectations.StoryExpectations):
      def SetExpectations(self):
        pass # Nothing.
    return StoryExpectations()


@benchmark.Owner(emails=['junov@chromium.org'])
class SmoothnessToughCanvasCases(_Smoothness):
  """Measures frame rate and a variety of other statistics.

  Uses a selection of pages making use of the 2D Canvas API.
  """
  page_set = page_sets.ToughCanvasCasesPageSet

  def SetExtraBrowserOptions(self, options):
    options.AppendExtraBrowserArgs('--enable-experimental-canvas-features')

  @classmethod
  def Name(cls):
    return 'smoothness.tough_canvas_cases'

  def GetExpectations(self):
    class StoryExpectations(story_module.expectations.StoryExpectations):
      def SetExpectations(self):
        # TODO(rnephew): Uncomment out after story is added back to set after
        # rerecording.
        # self.DisableStory(
        #     'http://ie.microsoft.com/testdrive/Performance/Fireflies/Default.'
        #     'html', [story_module.expectations.ALL], 'crbug.com/314131')

        # pylint: disable=line-too-long
        self.DisableStory('http://geoapis.appspot.com/agdnZW9hcGlzchMLEgtFeGFtcGxlQ29kZRjh1wIM',
                          [story_module.expectations.ANDROID_NEXUS5],
                          'crbug.com/364248')
        self.DisableStory('tough_canvas_cases/canvas_toBlob.html',
                          [story_module.expectations.ANDROID_ONE],
                          'crbug.com/755657')
    return StoryExpectations()


@benchmark.Owner(emails=['kbr@chromium.org', 'zmo@chromium.org'])
class SmoothnessToughWebGLCases(_Smoothness):
  page_set = page_sets.ToughWebglCasesPageSet

  @classmethod
  def Name(cls):
    return 'smoothness.tough_webgl_cases'

  def GetExpectations(self):
    class StoryExpectations(story_module.expectations.StoryExpectations):
      def SetExpectations(self):
        pass # Nothing.
    return StoryExpectations()


@benchmark.Owner(emails=['kbr@chromium.org', 'zmo@chromium.org'])
class SmoothnessMaps(perf_benchmark.PerfBenchmark):
  page_set = page_sets.MapsPageSet

  @classmethod
  def Name(cls):
    return 'smoothness.maps'

  def GetExpectations(self):
    class StoryExpectations(story_module.expectations.StoryExpectations):
      def SetExpectations(self):
        self.DisableStory(
            'http://map-test/performance.html',
            [story_module.expectations.ANDROID_WEBVIEW],
            'crbug.com/653993')
    return StoryExpectations()


@benchmark.Owner(emails=['ssid@chromium.org'])
class SmoothnessKeyDesktopMoveCases(_Smoothness):
  page_set = page_sets.KeyDesktopMoveCasesPageSet
  SUPPORTED_PLATFORMS = [story_module.expectations.ALL_DESKTOP]

  @classmethod
  def Name(cls):
    return 'smoothness.key_desktop_move_cases'

  def GetExpectations(self):
    class StoryExpectations(story_module.expectations.StoryExpectations):
      def SetExpectations(self):
        self.DisableStory('https://mail.google.com/mail/',
                          [story_module.expectations.ALL_WIN],
                          'crbug.com/750131')
        self.DisableStory('https://mail.google.com/mail/',
                          [story_module.expectations.ALL_MAC],
                          'crbug.com/770904')
    return StoryExpectations()


@benchmark.Owner(emails=['vmiura@chromium.org', 'tdresser@chromium.org'])
class SmoothnessKeyMobileSites(_Smoothness):
  """Measures rendering statistics while scrolling down the key mobile sites.

  http://www.chromium.org/developers/design-documents/rendering-benchmarks
  """
  page_set = page_sets.KeyMobileSitesSmoothPageSet
  SUPPORTED_PLATFORMS = [story_module.expectations.ALL_MOBILE]

  @classmethod
  def Name(cls):
    return 'smoothness.key_mobile_sites_smooth'

  def GetExpectations(self):
    class StoryExpectations(story_module.expectations.StoryExpectations):
      def SetExpectations(self):
        self.DisableStory(
            'http://digg.com',
            [story_module.expectations.ALL_ANDROID],
            'crbug.com/756119')
    return StoryExpectations()


@benchmark.Owner(emails=['alancutter@chromium.org'])
class SmoothnessToughAnimationCases(_Smoothness):
  page_set = page_sets.ToughAnimationCasesPageSet

  @classmethod
  def Name(cls):
    return 'smoothness.tough_animation_cases'

  def GetExpectations(self):
    class StoryExpectations(story_module.expectations.StoryExpectations):
      def SetExpectations(self):
        self.DisableStory('robohornetpro', [story_module.expectations.ALL],
                          'crbug.com/350692')
        self.DisableStory(
            'balls_css_keyframe_animations_composited_transform.html',
            [story_module.expectations.ALL_MOBILE],
            'crbug.com/755556')
        self.DisableStory(
            'mix_blend_mode_animation_difference.html',
            [story_module.expectations.ALL_MAC],
            'crbug.com/755556')
        self.DisableStory(
            'mix_blend_mode_animation_hue.html',
            [story_module.expectations.ALL_MAC],
            'crbug.com/755556')

    return StoryExpectations()


@benchmark.Owner(emails=['ajuma@chromium.org'])
class SmoothnessKeySilkCases(_Smoothness):
  """Measures rendering statistics for the key silk cases without GPU
  rasterization.
  """
  page_set = page_sets.KeySilkCasesPageSet
  SUPPORTED_PLATFORMS = [story_module.expectations.ALL_MOBILE]

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

  def GetExpectations(self):
    class StoryExpectations(story_module.expectations.StoryExpectations):
      def SetExpectations(self):
        self.DisableStory(
            'inbox_app.html?slide_drawer', [story_module.expectations.ALL],
            'Story contains multiple interaction records. Not supported in '
            'smoothness benchmarks.')
    return StoryExpectations()


@benchmark.Owner(emails=['vmiura@chromium.org'])
class SmoothnessGpuRasterizationTop25(_Smoothness):
  """Measures rendering statistics for the top 25 with GPU rasterization.
  """
  tag = 'gpu_rasterization'
  page_set = page_sets.Top25SmoothPageSet
  SUPPORTED_PLATFORMS = [story_module.expectations.ALL_MOBILE]

  def SetExtraBrowserOptions(self, options):
    silk_flags.CustomizeBrowserOptionsForGpuRasterization(options)

  @classmethod
  def Name(cls):
    return 'smoothness.gpu_rasterization.top_25_smooth'

  def GetExpectations(self):
    class StoryExpectations(story_module.expectations.StoryExpectations):
      def SetExpectations(self):
        self.DisableStory('http://www.cnn.com', [story_module.expectations.ALL],
                          'crbug.com/528474')
        self.DisableStory('http://www.amazon.com',
                          [story_module.expectations.ALL],
                          'crbug.com/667432')
        self.DisableStory('Pinterest', [story_module.expectations.ALL],
                          'crbug.com/667432')
    return StoryExpectations()


# Although GPU rasterization is enabled on Mac, it is blacklisted for certain
# path cases, so it is still valuable to run both the GPU and non-GPU versions
# of this benchmark on Mac.
@benchmark.Owner(emails=['senorblanco@chromium.org'])
class SmoothnessGpuRasterizationToughPathRenderingCases(_Smoothness):
  """Tests a selection of pages with SVG and 2D canvas paths with GPU
  rasterization.
  """
  tag = 'gpu_rasterization'
  page_set = page_sets.ToughPathRenderingCasesPageSet

  def SetExtraBrowserOptions(self, options):
    silk_flags.CustomizeBrowserOptionsForGpuRasterization(options)

  @classmethod
  def Name(cls):
    return 'smoothness.gpu_rasterization.tough_path_rendering_cases'

  def GetExpectations(self):
    class StoryExpectations(story_module.expectations.StoryExpectations):
      def SetExpectations(self):
        pass # Nothing.
    return StoryExpectations()


@benchmark.Owner(emails=['senorblanco@chromium.org'])
class SmoothnessGpuRasterizationFiltersCases(_Smoothness):
  """Tests a selection of pages with SVG and CSS filter effects with GPU
  rasterization.
  """
  tag = 'gpu_rasterization'
  page_set = page_sets.ToughFiltersCasesPageSet

  # With GPU Raster enabled on Mac, there's no reason to run this
  # benchmark in addition to SmoothnessFiltersCases.
  SUPPORTED_PLATFORMS = [
      story_module.expectations.ALL_LINUX,
      story_module.expectations.ALL_MOBILE,
      story_module.expectations.ALL_WIN
  ]

  def SetExtraBrowserOptions(self, options):
    silk_flags.CustomizeBrowserOptionsForGpuRasterization(options)

  @classmethod
  def Name(cls):
    return 'smoothness.gpu_rasterization.tough_filters_cases'

  def GetExpectations(self):
    class StoryExpectations(story_module.expectations.StoryExpectations):
      def SetExpectations(self):
        pass
    return StoryExpectations()


@benchmark.Owner(emails=['tdresser@chromium.org', 'rbyers@chromium.org'])
class SmoothnessSyncScrollKeyMobileSites(_Smoothness):
  """Measures rendering statistics for the key mobile sites with synchronous
  (main thread) scrolling.
  """
  tag = 'sync_scroll'
  page_set = page_sets.KeyMobileSitesSmoothPageSet
  SUPPORTED_PLATFORMS = [story_module.expectations.ALL_MOBILE]

  def SetExtraBrowserOptions(self, options):
    silk_flags.CustomizeBrowserOptionsForSyncScrolling(options)

  @classmethod
  def Name(cls):
    return 'smoothness.sync_scroll.key_mobile_sites_smooth'

  def GetExpectations(self):
    class StoryExpectations(story_module.expectations.StoryExpectations):
      def SetExpectations(self):
        self.DisableStory(
            'http://digg.com',
            [story_module.expectations.ALL],
            'crbug.com/756119')
    return StoryExpectations()


@benchmark.Owner(emails=['vmiura@chromium.org'])
class SmoothnessSimpleMobilePages(_Smoothness):
  """Measures rendering statistics for simple mobile sites page set.
  """
  page_set = page_sets.SimpleMobileSitesPageSet
  SUPPORTED_PLATFORMS = [story_module.expectations.ALL_MOBILE]

  @classmethod
  def Name(cls):
    return 'smoothness.simple_mobile_sites'

  def GetExpectations(self):
    class StoryExpectations(story_module.expectations.StoryExpectations):
      def SetExpectations(self):
        self.DisableStory('https://www.flickr.com/',
                          [story_module.expectations.ANDROID_WEBVIEW],
                          'crbug.com/750833')
    return StoryExpectations()


@benchmark.Owner(emails=['bokan@chromium.org'])
class SmoothnessToughPinchZoomCases(_Smoothness):
  """Measures rendering statistics for pinch-zooming in the tough pinch zoom
  cases.
  """
  page_set = page_sets.AndroidToughPinchZoomCasesPageSet
  SUPPORTED_PLATFORMS = [story_module.expectations.ALL_MOBILE]

  @classmethod
  def Name(cls):
    return 'smoothness.tough_pinch_zoom_cases'

  def GetExpectations(self):
    class StoryExpectations(story_module.expectations.StoryExpectations):
      def SetExpectations(self):
        self.DisableStory('https://www.google.com/#hl=en&q=barack+obama',
                          [story_module.expectations.ALL_ANDROID],
                          'crbug.com/631015')
        self.DisableStory('https://mail.google.com/mail/',
                          [story_module.expectations.ALL_ANDROID],
                          'crbug.com/631015')
        self.DisableStory('https://www.google.com/calendar/',
                          [story_module.expectations.ALL_ANDROID],
                          'crbug.com/631015')
        self.DisableStory('https://www.google.com/search?q=cats&tbm=isch',
                          [story_module.expectations.ALL_ANDROID],
                          'crbug.com/631015')
        self.DisableStory('http://www.youtube.com',
                          [story_module.expectations.ALL_ANDROID],
                          'crbug.com/631015')
        self.DisableStory('Blogger',
                          [story_module.expectations.ALL_ANDROID],
                          'crbug.com/631015')
        self.DisableStory('Facebook',
                          [story_module.expectations.ALL_ANDROID],
                          'crbug.com/631015')
        self.DisableStory('LinkedIn',
                          [story_module.expectations.ALL_ANDROID],
                          'crbug.com/631015')
        self.DisableStory('Twitter',
                          [story_module.expectations.ALL_ANDROID],
                          'crbug.com/631015')
        self.DisableStory('ESPN',
                          [story_module.expectations.ALL_ANDROID],
                          'crbug.com/631015')
        self.DisableStory('http://news.yahoo.com',
                          [story_module.expectations.ALL_ANDROID],
                          'crbug.com/631015')
        self.DisableStory('http://www.cnn.com',
                          [story_module.expectations.ALL_ANDROID],
                          'crbug.com/631015')
        self.DisableStory('Weather.com',
                          [story_module.expectations.ALL_ANDROID],
                          'crbug.com/631015')
        self.DisableStory('http://www.amazon.com',
                          [story_module.expectations.ALL_ANDROID],
                          'crbug.com/631015')
        self.DisableStory('http://www.ebay.com',
                          [story_module.expectations.ALL_ANDROID],
                          'crbug.com/631015')
        self.DisableStory('http://games.yahoo.com',
                          [story_module.expectations.ALL_ANDROID],
                          'crbug.com/631015')
        self.DisableStory('http://booking.com',
                          [story_module.expectations.ALL_ANDROID],
                          'crbug.com/631015')
        self.DisableStory('http://sports.yahoo.com/',
                          [story_module.expectations.ALL_ANDROID],
                          'crbug.com/631015')
    return StoryExpectations()


@benchmark.Owner(emails=['ericrk@chromium.org'])
class SmoothnessDesktopToughPinchZoomCases(_Smoothness):
  """Measures rendering statistics for pinch-zooming in the tough pinch zoom
  cases. Uses lower zoom levels customized for desktop limits.
  """
  page_set = page_sets.DesktopToughPinchZoomCasesPageSet
  SUPPORTED_PLATFORMS = [story_module.expectations.ALL_MAC]

  @classmethod
  def Name(cls):
    return 'smoothness.desktop_tough_pinch_zoom_cases'

  def GetExpectations(self):
    class StoryExpectations(story_module.expectations.StoryExpectations):
      def SetExpectations(self):
        pass  # Nothing disabled.
    return StoryExpectations()


@benchmark.Owner(emails=['ericrk@chromium.org'])
class SmoothnessGpuRasterizationToughPinchZoomCases(_Smoothness):
  """Measures rendering statistics for pinch-zooming in the tough pinch zoom
  cases with GPU rasterization.
  """
  tag = 'gpu_rasterization'
  test = smoothness.Smoothness
  page_set = page_sets.AndroidToughPinchZoomCasesPageSet
  SUPPORTED_PLATFORMS = [story_module.expectations.ALL_MOBILE]

  def SetExtraBrowserOptions(self, options):
    silk_flags.CustomizeBrowserOptionsForGpuRasterization(options)

  @classmethod
  def Name(cls):
    return 'smoothness.gpu_rasterization.tough_pinch_zoom_cases'

  def GetExpectations(self):

    class StoryExpectations(story_module.expectations.StoryExpectations):

      def SetExpectations(self):
        self.DisableStory('https://www.google.com/#hl=en&q=barack+obama',
                          [story_module.expectations.ALL_ANDROID],
                          'crbug.com/610021')
        self.DisableStory('https://mail.google.com/mail/',
                          [story_module.expectations.ALL_ANDROID],
                          'crbug.com/610021')
        self.DisableStory('https://www.google.com/calendar/',
                          [story_module.expectations.ALL_ANDROID],
                          'crbug.com/610021')
        self.DisableStory('https://www.google.com/search?q=cats&tbm=isch',
                          [story_module.expectations.ALL_ANDROID],
                          'crbug.com/610021')
        self.DisableStory('http://www.youtube.com',
                          [story_module.expectations.ALL_ANDROID],
                          'crbug.com/610021')
        self.DisableStory('Blogger', [story_module.expectations.ALL_ANDROID],
                          'crbug.com/610021')
        self.DisableStory('Facebook', [story_module.expectations.ALL_ANDROID],
                          'crbug.com/610021')
        self.DisableStory('LinkedIn', [story_module.expectations.ALL_ANDROID],
                          'crbug.com/610021')
        self.DisableStory('Twitter', [story_module.expectations.ALL_ANDROID],
                          'crbug.com/610021')
        self.DisableStory('ESPN', [story_module.expectations.ALL_ANDROID],
                          'crbug.com/610021')
        self.DisableStory('http://news.yahoo.com',
                          [story_module.expectations.ALL_ANDROID],
                          'crbug.com/610021')
        self.DisableStory('http://www.cnn.com',
                          [story_module.expectations.ALL_ANDROID],
                          'crbug.com/610021')
        self.DisableStory('Weather.com',
                          [story_module.expectations.ALL_ANDROID],
                          'crbug.com/610021')
        self.DisableStory('http://www.amazon.com',
                          [story_module.expectations.ALL_ANDROID],
                          'crbug.com/610021')
        self.DisableStory('http://www.ebay.com',
                          [story_module.expectations.ALL_ANDROID],
                          'crbug.com/610021')
        self.DisableStory('http://games.yahoo.com',
                          [story_module.expectations.ALL_ANDROID],
                          'crbug.com/610021')
        self.DisableStory('http://booking.com',
                          [story_module.expectations.ALL_ANDROID],
                          'crbug.com/610021')
        self.DisableStory('http://sports.yahoo.com/',
                          [story_module.expectations.ALL_ANDROID],
                          'crbug.com/610021')

    return StoryExpectations()


@benchmark.Owner(emails=['vmiura@chromium.org'])
class SmoothnessGpuRasterizationPolymer(_Smoothness):
  """Measures rendering statistics for the Polymer cases with GPU rasterization.
  """
  tag = 'gpu_rasterization'
  page_set = page_sets.PolymerPageSet

  def SetExtraBrowserOptions(self, options):
    silk_flags.CustomizeBrowserOptionsForGpuRasterization(options)

  @classmethod
  def Name(cls):
    return 'smoothness.gpu_rasterization.polymer'

  def GetExpectations(self):
    class StoryExpectations(story_module.expectations.StoryExpectations):
      def SetExpectations(self):
        self.DisableBenchmark(
            [story_module.expectations.ALL],
            'Mobile Benchmark that needs modernization. Crbug.com/750876')
    return StoryExpectations()


@benchmark.Owner(emails=['reveman@chromium.org'])
class SmoothnessToughScrollingCases(_Smoothness):
  page_set = page_sets.ToughScrollingCasesPageSet

  @classmethod
  def ValueCanBeAddedPredicate(cls, value, is_first_result):
    del is_first_result  # unused
    # Only keep 'mean_pixels_approximated' and 'mean_pixels_checkerboarded'
    # metrics. (crbug.com/529331)
    return value.name in ('mean_pixels_approximated',
                          'mean_pixels_checkerboarded')

  @classmethod
  def Name(cls):
    return 'smoothness.tough_scrolling_cases'

  def GetExpectations(self):
    class StoryExpectations(story_module.expectations.StoryExpectations):
      def SetExpectations(self):
        pass # Nothing.
    return StoryExpectations()


@benchmark.Owner(emails=['ericrk@chromium.org'])
class SmoothnessGpuRasterizationToughScrollingCases(_Smoothness):
  tag = 'gpu_rasterization'
  test = smoothness.Smoothness
  page_set = page_sets.ToughScrollingCasesPageSet

  def SetExtraBrowserOptions(self, options):
    silk_flags.CustomizeBrowserOptionsForGpuRasterization(options)

  @classmethod
  def Name(cls):
    return 'smoothness.gpu_rasterization.tough_scrolling_cases'

  def GetExpectations(self):
    class StoryExpectations(story_module.expectations.StoryExpectations):
      def SetExpectations(self):
        pass # Nothing.
    return StoryExpectations()


class SmoothnessToughImageDecodeCases(_Smoothness):
  page_set = page_sets.ToughImageDecodeCasesPageSet

  @classmethod
  def Name(cls):
    return 'smoothness.tough_image_decode_cases'

  def GetExpectations(self):
    class StoryExpectations(story_module.expectations.StoryExpectations):
      def SetExpectations(self):
        pass # Nothing.
    return StoryExpectations()


@benchmark.Owner(emails=['cblume@chromium.org'])
class SmoothnessImageDecodingCases(_Smoothness):
  """Measures decoding statistics for jpeg images.
  """
  page_set = page_sets.ImageDecodingCasesPageSet

  def SetExtraBrowserOptions(self, options):
    silk_flags.CustomizeBrowserOptionsForGpuRasterization(options)
    options.AppendExtraBrowserArgs('--disable-accelerated-jpeg-decoding')

  @classmethod
  def Name(cls):
    return 'smoothness.image_decoding_cases'

  def GetExpectations(self):
    class StoryExpectations(story_module.expectations.StoryExpectations):
      def SetExpectations(self):
        pass # Nothing.
    return StoryExpectations()


@benchmark.Owner(emails=['cblume@chromium.org'])
class SmoothnessGpuImageDecodingCases(_Smoothness):
  """Measures decoding statistics for jpeg images with GPU rasterization.
  """
  tag = 'gpu_rasterization_and_decoding'
  page_set = page_sets.ImageDecodingCasesPageSet

  def SetExtraBrowserOptions(self, options):
    silk_flags.CustomizeBrowserOptionsForGpuRasterization(options)
    # TODO(sugoi): Remove the following line once M41 goes stable
    options.AppendExtraBrowserArgs('--enable-accelerated-jpeg-decoding')

  @classmethod
  def Name(cls):
    return 'smoothness.gpu_rasterization_and_decoding.image_decoding_cases'

  def GetExpectations(self):
    class StoryExpectations(story_module.expectations.StoryExpectations):
      def SetExpectations(self):
        pass # Nothing.
    return StoryExpectations()


@benchmark.Owner(emails=['picksi@chromium.org'])
class SmoothnessPathologicalMobileSites(_Smoothness):
  """Measures task execution statistics while scrolling pathological sites.
  """
  page_set = page_sets.PathologicalMobileSitesPageSet
  SUPPORTED_PLATFORMS = [story_module.expectations.ALL_MOBILE]

  @classmethod
  def Name(cls):
    return 'smoothness.pathological_mobile_sites'

  def GetExpectations(self):
    class StoryExpectations(story_module.expectations.StoryExpectations):
      def SetExpectations(self):
        self.DisableBenchmark([story_module.expectations.ANDROID_NEXUS7],
                              'crbug.com/685342')
    return StoryExpectations()


@benchmark.Owner(emails=['vmiura@chromium.org'])
class SmoothnessToughTextureUploadCases(_Smoothness):
  page_set = page_sets.ToughTextureUploadCasesPageSet

  @classmethod
  def Name(cls):
    return 'smoothness.tough_texture_upload_cases'

  def GetExpectations(self):
    class StoryExpectations(story_module.expectations.StoryExpectations):
      pass # Nothing.
    return StoryExpectations()


@benchmark.Owner(emails=['skyostil@chromium.org'])
class SmoothnessToughAdCases(_Smoothness):
  """Measures rendering statistics while displaying advertisements."""
  page_set = page_sets.SyntheticToughAdCasesPageSet

  @classmethod
  def Name(cls):
    return 'smoothness.tough_ad_cases'

  @classmethod
  def ValueCanBeAddedPredicate(cls, value, is_first_result):
    del is_first_result  # unused
    # These pages don't scroll so it's not necessary to measure input latency.
    return value.name != 'first_gesture_scroll_update_latency'

  def GetExpectations(self):
    class StoryExpectations(story_module.expectations.StoryExpectations):
      def SetExpectations(self):
        self.DisableBenchmark([story_module.expectations.ANDROID_SVELTE],
                              'www.crbug.com/555089')
    return StoryExpectations()


@benchmark.Owner(emails=['skyostil@chromium.org'])
class SmoothnessToughWebGLAdCases(_Smoothness):
  """Measures rendering statistics while scrolling advertisements."""
  page_set = page_sets.SyntheticToughWebglAdCasesPageSet

  @classmethod
  def Name(cls):
    return 'smoothness.tough_webgl_ad_cases'

  def GetExpectations(self):
    class StoryExpectations(story_module.expectations.StoryExpectations):
      def SetExpectations(self):
        self.DisableBenchmark([story_module.expectations.ANDROID_SVELTE],
                              'crbug.com/574485')
    return StoryExpectations()


@benchmark.Owner(emails=['skyostil@chromium.org', 'brianderson@chromium.org'])
class SmoothnessToughSchedulingCases(_Smoothness):
  """Measures rendering statistics while interacting with pages that have
  challenging scheduling properties.

  https://docs.google.com/a/chromium.org/document/d/
      17yhE5Po9By0sCdM1yZT3LiUECaUr_94rQt9j-4tOQIM/view"""
  page_set = page_sets.ToughSchedulingCasesPageSet

  @classmethod
  def Name(cls):
    # This should be smoothness.tough_scheduling_cases but since the benchmark
    # has been named this way for long time, we keep the name as-is to avoid
    # data migration.
    return 'scheduler.tough_scheduling_cases'
