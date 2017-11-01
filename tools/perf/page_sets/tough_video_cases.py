# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from telemetry.page import page as page_module
from telemetry.page import traffic_setting as traffic_setting_module
from telemetry import story

# A complete list of page tags to check. This prevents misspellings and provides
# documentation of scenarios for code readers.
_PAGE_TAGS_LIST = [
    # Audio codecs.
    'pcm',
    'mp3',
    'aac',
    'vorbis',
    'opus',
    # Video codecs.
    'h264',
    'vp8',
    'vp9',
    # Test types.
    'audio_video',
    'audio_only',
    'video_only',
    # Other filter tags.
    'is_50fps',
    'is_4k',
    # Play action.
    'seek',
    'beginning_to_end',
    'background',
    # Add javascript load.
    'busyjs',
    # Constrained network settings.
    'cns',
    # VideoStack API.
    'src',
    'mse'
]

# A list of traffic setting names to append to page names when used.
# Traffic settings is a way to constrain the network to match real-world
# scenarios.
_TRAFFIC_SETTING_NAMES = {
    traffic_setting_module.GPRS: 'GPRS',
    traffic_setting_module.REGULAR_2G: 'Regular-2G',
    traffic_setting_module.GOOD_2G: 'Good-2G',
    traffic_setting_module.REGULAR_3G: 'Regular-3G',
    traffic_setting_module.GOOD_3G: 'Good-3G',
    traffic_setting_module.REGULAR_4G: 'Regular-4G',
    traffic_setting_module.DSL: 'DSL',
    traffic_setting_module.WIFI: 'WiFi',
}

#
# The following section contains base classes for pages.
#

class MediaPage(page_module.Page):

  def __init__(self, url, page_set, tags, extra_browser_args=None,
               traffic_setting=traffic_setting_module.NONE):
    name = url.split('/')[-1]
    if traffic_setting != traffic_setting_module.NONE:
      name += '_' + _TRAFFIC_SETTING_NAMES[traffic_setting]
      tags.append('cns')
    if tags:
      for t in tags:
        assert t in _PAGE_TAGS_LIST
    assert not ('src' in tags and 'mse' in tags)
    super(MediaPage, self).__init__(
        url=url, page_set=page_set, tags=tags, name=name,
        extra_browser_args=extra_browser_args,
        traffic_setting=traffic_setting)


class BeginningToEndPlayPage(MediaPage):
  """A normal play page simply plays the given media until the end."""

  def __init__(self, url, page_set, tags, extra_browser_args=None,
               traffic_setting=traffic_setting_module.NONE):
    tags.append('beginning_to_end')
    tags.append('src')
    self.add_browser_metrics = True
    super(BeginningToEndPlayPage, self).__init__(
        url, page_set, tags, extra_browser_args,
        traffic_setting=traffic_setting)

  def RunPageInteractions(self, action_runner):
    # Play the media until it has finished or it times out.
    action_runner.PlayMedia(playing_event_timeout_in_seconds=60,
                            ended_event_timeout_in_seconds=60)
    # Generate memory dump for memoryMetric.
    if self.page_set.measure_memory:
      action_runner.MeasureMemory()

class SeekPage(MediaPage):
  """A seek page seeks twice in the video and measures the seek time."""

  def __init__(self, url, page_set, tags, extra_browser_args=None,
               action_timeout_in_seconds=60,
               traffic_setting=traffic_setting_module.NONE):
    tags.append('seek')
    tags.append('src')
    self.skip_basic_metrics = True
    self._action_timeout = action_timeout_in_seconds
    super(SeekPage, self).__init__(
        url, page_set, tags, extra_browser_args,
        traffic_setting=traffic_setting)

  def RunPageInteractions(self, action_runner):
    timeout = self._action_timeout
    # Start the media playback.
    action_runner.PlayMedia(
        playing_event_timeout_in_seconds=timeout)
    # Wait for 1 second so that we know the play-head is at ~1s.
    action_runner.Wait(1)
    # Seek to before the play-head location.
    action_runner.SeekMedia(seconds=0.5, timeout_in_seconds=timeout,
                            label='seek_warm')
    # Seek to after the play-head location.
    action_runner.SeekMedia(seconds=9, timeout_in_seconds=timeout,
                            label='seek_cold')
    # Generate memory dump for memoryMetric.
    if self.page_set.measure_memory:
      action_runner.MeasureMemory()

class BackgroundPlaybackPage(MediaPage):
  """A Background playback page plays the given media in a background tab.

  The motivation for this test case is crbug.com/678663.
  """

  def __init__(self, url, page_set, tags, extra_browser_args=None,
               background_time=10,
               traffic_setting=traffic_setting_module.NONE):
    self._background_time = background_time
    self.skip_basic_metrics = True
    tags.append('background')
    tags.append('src')
    # disable-media-suspend is required since for Android background playback
    # gets suspended. This flag makes Android work the same way as desktop and
    # not turn off video playback in the background.
    extra_browser_args = extra_browser_args or []
    extra_browser_args.append('--disable-media-suspend')
    super(BackgroundPlaybackPage, self).__init__(
        url, page_set, tags, extra_browser_args)

  def RunPageInteractions(self, action_runner):
    # Steps:
    # 1. Play a video
    # 2. Open new tab overtop to obscure the video
    # 3. Close the tab to go back to the tab that is playing the video.
    action_runner.PlayMedia(
        playing_event_timeout_in_seconds=60)
    action_runner.Wait(.5)
    new_tab = action_runner.tab.browser.tabs.New()
    new_tab.Activate()
    action_runner.Wait(self._background_time)
    new_tab.Close()
    action_runner.Wait(.5)
    # Generate memory dump for memoryMetric.
    if self.page_set.measure_memory:
      action_runner.MeasureMemory()


class MSEPage(MediaPage):

  def __init__(self, url, page_set, tags, extra_browser_args=None):
    tags.append('mse')
    super(MSEPage, self).__init__(
        url, page_set, tags, extra_browser_args)

  def RunPageInteractions(self, action_runner):
    # The page automatically runs the test at load time.
    action_runner.WaitForJavaScriptCondition('window.__testDone == true')
    test_failed = action_runner.EvaluateJavaScript('window.__testFailed')
    if test_failed:
      raise RuntimeError(action_runner.EvaluateJavaScript('window.__testError'))


#
# The following section contains concrete test page definitions.
#

class Page2(BeginningToEndPlayPage):

  def __init__(self, page_set):
    super(Page2, self).__init__(
      url='file://tough_video_cases/video.html?src=crowd.ogg&type=audio',
      page_set=page_set,
      tags=['vorbis', 'audio_only'])


class Page4(BeginningToEndPlayPage):

  def __init__(self, page_set):
    super(Page4, self).__init__(
      url='file://tough_video_cases/video.html?src=crowd1080.webm',
      page_set=page_set,
      tags=['is_50fps', 'vp8', 'vorbis', 'audio_video'])


class Page7(BeginningToEndPlayPage):

  def __init__(self, page_set):
    super(Page7, self).__init__(
      url='file://tough_video_cases/video.html?src=tulip2.ogg&type=audio',
      page_set=page_set,
      tags=['vorbis', 'audio_only'])


class Page8(BeginningToEndPlayPage):

  def __init__(self, page_set):
    super(Page8, self).__init__(
      url='file://tough_video_cases/video.html?src=tulip2.wav&type=audio',
      page_set=page_set,
      tags=['pcm', 'audio_only'])


class Page11(BeginningToEndPlayPage):

  def __init__(self, page_set):
    super(Page11, self).__init__(
      url='file://tough_video_cases/video.html?src=crowd1080.mp4',
      page_set=page_set,
      tags=['is_50fps', 'h264', 'aac', 'audio_video'])


class Page12(BeginningToEndPlayPage):

  def __init__(self, page_set):
    super(Page12, self).__init__(
      url='file://tough_video_cases/video.html?src=crowd2160.mp4',
      page_set=page_set,
      tags=['is_4k', 'is_50fps', 'h264', 'aac', 'audio_video'])


class Page13(BeginningToEndPlayPage):

  def __init__(self, page_set):
    super(Page13, self).__init__(
      url='file://tough_video_cases/video.html?src=tulip2.mp3&type=audio',
      page_set=page_set,
      tags=['mp3', 'audio_only'])


class Page14(BeginningToEndPlayPage):

  def __init__(self, page_set):
    super(Page14, self).__init__(
      url='file://tough_video_cases/video.html?src=tulip2.mp4',
      page_set=page_set,
      tags=['h264', 'aac', 'audio_video'])


class Page15(BeginningToEndPlayPage):

  def __init__(self, page_set):
    super(Page15, self).__init__(
      url='file://tough_video_cases/video.html?src=tulip2.m4a&type=audio',
      page_set=page_set,
      tags=['aac', 'audio_only'])


class Page16(BeginningToEndPlayPage):

  def __init__(self, page_set):
    super(Page16, self).__init__(
      url='file://tough_video_cases/video.html?src=garden2_10s.webm',
      page_set=page_set,
      tags=['is_4k', 'vp8', 'vorbis', 'audio_video'])


class Page17(BeginningToEndPlayPage):

  def __init__(self, page_set):
    super(Page17, self).__init__(
      url='file://tough_video_cases/video.html?src=garden2_10s.mp4',
      page_set=page_set,
      tags=['is_4k', 'h264', 'aac', 'audio_video'])


class Page19(SeekPage):

  def __init__(self, page_set):
    super(Page19, self).__init__(
      url='file://tough_video_cases/video.html?src=tulip2.ogg&type=audio&seek',
      page_set=page_set,
      tags=['vorbis', 'audio_only'])


class Page20(SeekPage):

  def __init__(self, page_set):
    super(Page20, self).__init__(
      url='file://tough_video_cases/video.html?src=tulip2.wav&type=audio&seek',
      page_set=page_set,
      tags=['pcm', 'audio_only'])


class Page23(SeekPage):

  def __init__(self, page_set):
    super(Page23, self).__init__(
      url='file://tough_video_cases/video.html?src=tulip2.mp3&type=audio&seek',
      page_set=page_set,
      tags=['mp3', 'audio_only'])


class Page24(SeekPage):

  def __init__(self, page_set):
    super(Page24, self).__init__(
      url='file://tough_video_cases/video.html?src=tulip2.mp4&seek',
      page_set=page_set,
      tags=['h264', 'aac', 'audio_video'])


class Page25(SeekPage):

  def __init__(self, page_set):
    super(Page25, self).__init__(
      url='file://tough_video_cases/video.html?src=garden2_10s.webm&seek',
      page_set=page_set,
      tags=['is_4k', 'vp8', 'vorbis', 'audio_video'])


class Page26(SeekPage):

  def __init__(self, page_set):
    super(Page26, self).__init__(
      url='file://tough_video_cases/video.html?src=garden2_10s.mp4&seek',
      page_set=page_set,
      tags=['is_4k', 'h264', 'aac', 'audio_video'])


class Page30(BeginningToEndPlayPage):

  def __init__(self, page_set):
    super(Page30, self).__init__(
      url='file://tough_video_cases/video.html?src=tulip2.vp9.webm',
      page_set=page_set,
      tags=['vp9', 'opus', 'audio_video'])


class Page31(SeekPage):

  def __init__(self, page_set):
    super(Page31, self).__init__(
      url='file://tough_video_cases/video.html?src=tulip2.vp9.webm&seek',
      page_set=page_set,
      tags=['vp9', 'opus', 'audio_video'])


class Page32(BeginningToEndPlayPage):

  def __init__(self, page_set):
    super(Page32, self).__init__(
      url='file://tough_video_cases/video.html?src=crowd1080_vp9.webm',
      page_set=page_set,
      tags=['vp9', 'video_only'])


class Page33(SeekPage):

  def __init__(self, page_set):
    super(Page33, self).__init__(
      url='file://tough_video_cases/video.html?src=crowd1080_vp9.webm&seek',
      page_set=page_set,
      tags=['vp9', 'video_only', 'seek'])


class Page34(BeginningToEndPlayPage):

  def __init__(self, page_set):
    super(Page34, self).__init__(
      url='file://tough_video_cases/video.html?src=crowd720_vp9.webm',
      page_set=page_set,
      tags=['vp9', 'video_only'])


class Page36(SeekPage):

  def __init__(self, page_set):
    super(Page36, self).__init__(
      url=('file://tough_video_cases/video.html?src='
           'smpte_3840x2160_60fps_vp9.webm&seek'),
      page_set=page_set,
      tags=['is_4k', 'vp9', 'video_only'],
      action_timeout_in_seconds=120)


class Page37(BackgroundPlaybackPage):

  def __init__(self, page_set):
    super(Page37, self).__init__(
      url='file://tough_video_cases/video.html?src=tulip2.vp9.webm&background',
      page_set=page_set,
      tags=['vp9', 'opus', 'audio_video'])


class Page38(BeginningToEndPlayPage):

  def __init__(self, page_set):
    super(Page38, self).__init__(
      url='file://tough_video_cases/video.html?src=tulip2.mp4&busyjs',
      page_set=page_set,
      tags=['h264', 'aac', 'audio_video', 'busyjs'])


class Page39(MSEPage):

  def __init__(self, page_set):
    super(Page39, self).__init__(
      url='file://tough_video_cases/mse.html?media=aac_audio.mp4,h264_video.mp4',
      page_set=page_set,
      tags=['h264', 'aac', 'audio_video'])


class Page40(MSEPage):

  def __init__(self, page_set):
    super(Page40, self).__init__(
      url=('file://tough_video_cases/mse.html?'
           'media=aac_audio.mp4,h264_video.mp4&waitForPageLoaded=true'),
      page_set=page_set,
      tags=['h264', 'aac', 'audio_video'])


class Page41(MSEPage):

  def __init__(self, page_set):
    super(Page41, self).__init__(
      url='file://tough_video_cases/mse.html?media=aac_audio.mp4',
      page_set=page_set,
      tags=['aac', 'audio_only'])


class Page42(MSEPage):

  def __init__(self, page_set):
    super(Page42, self).__init__(
      url='file://tough_video_cases/mse.html?media=h264_video.mp4',
      page_set=page_set,
      tags=['h264', 'video_only'])


class Page43(BeginningToEndPlayPage):

  def __init__(self, page_set):
    super(Page43, self).__init__(
      url='file://tough_video_cases/video.html?src=tulip2.vp9.webm',
      page_set=page_set,
      tags=['vp9', 'opus', 'audio_video'],
      traffic_setting=traffic_setting_module.REGULAR_3G)


class ToughVideoCasesPageSet(story.StorySet):
  """
  Description: Video Stack Perf pages that report time_to_play, seek time and
  many other media-specific and generic metric.

  TODO(crbug/713335): Rename this to MediaStorySet.
  """
  def __init__(self, measure_memory=False):
    super(ToughVideoCasesPageSet, self).__init__(
            cloud_storage_bucket=story.PARTNER_BUCKET)

    self.measure_memory = measure_memory

    # TODO(crouleau): Clean up this PageX stuff. Either
    # 1. Add magic that will add all pages to the StorySet
    # automatically so that the following is unnecessary. See
    # https://stackoverflow.com/questions/1796180. Just add all classes with
    # name like "PageX" where X is a number.
    # or
    # 2. Don't use classes at all and instead just have a list containing
    # configuration for each one.

    # Normal play tests.
    self.AddStory(Page2(self))
    self.AddStory(Page4(self))
    self.AddStory(Page7(self))
    self.AddStory(Page8(self))
    self.AddStory(Page11(self))
    self.AddStory(Page12(self))
    self.AddStory(Page13(self))
    self.AddStory(Page14(self))
    self.AddStory(Page15(self))
    self.AddStory(Page16(self))
    self.AddStory(Page17(self))
    self.AddStory(Page30(self))
    self.AddStory(Page32(self))
    self.AddStory(Page34(self))

    # Seek tests.
    self.AddStory(Page19(self))
    self.AddStory(Page20(self))
    self.AddStory(Page23(self))
    self.AddStory(Page24(self))
    self.AddStory(Page25(self))
    self.AddStory(Page26(self))
    self.AddStory(Page31(self))
    self.AddStory(Page33(self))
    self.AddStory(Page36(self))

    # Background playback tests.
    self.AddStory(Page37(self))

    # Tests with high JS load.
    self.AddStory(Page38(self))

    # Tests with a simulated constrained network connection.
    self.AddStory(Page43(self))

    # MSE tests.
    # TODO(crouleau): Figure out a way to make MSE pages provide consistent
    # data. Currently the data haa a lot of outliers for time_to_play and other
    # startup metrics. To do this, we must either:
    # 1. Tell Telemetry to repeat these pages (this requires Telemetry's
    # providing this option in the API.)
    # 2. Edit the page to reload and run multiple times (you can clear the cache
    # with tab.CleanCache). This requires crbug/775264.
    self.AddStory(Page39(self))
    self.AddStory(Page40(self))
    self.AddStory(Page41(self))
    self.AddStory(Page42(self))
