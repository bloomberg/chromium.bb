# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
from telemetry.page import page as page_module
from telemetry.page import page_set as page_set_module


class ToughVideoCasesPage(page_module.Page):

  def __init__(self, url, page_set, labels=None):
    super(ToughVideoCasesPage, self).__init__(
        url=url, page_set=page_set, labels=labels)

  def LoopMixedAudio(self, action_runner):
    action_runner.PlayMedia(selector='#background_audio',
                            playing_event_timeout_in_seconds=60)
    action_runner.LoopMedia(loop_count=50, selector='#mixed_audio')

  def LoopSingleAudio(self, action_runner):
    action_runner.LoopMedia(loop_count=50, selector='#single_audio')

  def PlayAction(self, action_runner):
    action_runner.PlayMedia(playing_event_timeout_in_seconds=60,
                            ended_event_timeout_in_seconds=60)

  def SeekBeforeAndAfterPlayhead(self, action_runner):
    action_runner.PlayMedia(playing_event_timeout_in_seconds=60,
                            ended_event_timeout_in_seconds=60)
    # Wait for 1 second so that we know the play-head is at ~1s.
    action_runner.Wait(1)
    # Seek to before the play-head location.
    action_runner.SeekMedia(seconds=0.5, timeout_in_seconds=60,
                            label='seek_warm')
    # Seek to after the play-head location.
    action_runner.SeekMedia(seconds=9, timeout_in_seconds=60,
                            label='seek_cold')


class Page1(ToughVideoCasesPage):

  def __init__(self, page_set):
    super(Page1, self).__init__(
      url='file://tough_video_cases/video.html?src=crowd.wav&type=audio',
      page_set=page_set)

    self.add_browser_metrics = True

  def RunPageInteractions(self, action_runner):
    self.PlayAction(action_runner)


class Page2(ToughVideoCasesPage):

  def __init__(self, page_set):
    super(Page2, self).__init__(
      url='file://tough_video_cases/video.html?src=crowd.ogg&type=audio',
      page_set=page_set)

    self.add_browser_metrics = True

  def RunPageInteractions(self, action_runner):
    self.PlayAction(action_runner)


class Page3(ToughVideoCasesPage):

  def __init__(self, page_set):
    super(Page3, self).__init__(
      url='file://tough_video_cases/video.html?src=crowd1080.ogv',
      page_set=page_set)

    self.add_browser_metrics = True
    self.is_50fps = True

  def RunPageInteractions(self, action_runner):
    self.PlayAction(action_runner)


class Page4(ToughVideoCasesPage):

  def __init__(self, page_set):
    super(Page4, self).__init__(
      url='file://tough_video_cases/video.html?src=crowd1080.webm',
      page_set=page_set, labels=['is_50fps'])

    self.add_browser_metrics = True

  def RunPageInteractions(self, action_runner):
    self.PlayAction(action_runner)


class Page5(ToughVideoCasesPage):

  def __init__(self, page_set):
    super(Page5, self).__init__(
      url='file://tough_video_cases/video.html?src=crowd2160.ogv',
      page_set=page_set, labels=['is_4k', 'is_50fps'])

    self.add_browser_metrics = True

  def RunPageInteractions(self, action_runner):
    self.PlayAction(action_runner)


class Page6(ToughVideoCasesPage):

  def __init__(self, page_set):
    super(Page6, self).__init__(
      url='file://tough_video_cases/video.html?src=crowd2160.webm',
      page_set=page_set, labels=['is_4k', 'is_50fps'])

    self.add_browser_metrics = True

  def RunPageInteractions(self, action_runner):
    self.PlayAction(action_runner)


class Page7(ToughVideoCasesPage):

  def __init__(self, page_set):
    super(Page7, self).__init__(
      url='file://tough_video_cases/video.html?src=tulip2.ogg&type=audio',
      page_set=page_set)

    self.add_browser_metrics = True

  def RunPageInteractions(self, action_runner):
    self.PlayAction(action_runner)


class Page8(ToughVideoCasesPage):

  def __init__(self, page_set):
    super(Page8, self).__init__(
      url='file://tough_video_cases/video.html?src=tulip2.wav&type=audio',
      page_set=page_set)

    self.add_browser_metrics = True

  def RunPageInteractions(self, action_runner):
    self.PlayAction(action_runner)


class Page9(ToughVideoCasesPage):

  def __init__(self, page_set):
    super(Page9, self).__init__(
      url='file://tough_video_cases/video.html?src=tulip2.ogv',
      page_set=page_set)

    self.add_browser_metrics = True

  def RunPageInteractions(self, action_runner):
    self.PlayAction(action_runner)


class Page10(ToughVideoCasesPage):

  def __init__(self, page_set):
    super(Page10, self).__init__(
      url='file://tough_video_cases/video.html?src=tulip2.webm',
      page_set=page_set)

    self.add_browser_metrics = True

  def RunPageInteractions(self, action_runner):
    self.PlayAction(action_runner)


class Page11(ToughVideoCasesPage):

  def __init__(self, page_set):
    super(Page11, self).__init__(
      url='file://tough_video_cases/video.html?src=crowd1080.mp4',
      page_set=page_set, labels=['is_50fps'])

    self.add_browser_metrics = True

  def RunPageInteractions(self, action_runner):
    self.PlayAction(action_runner)


class Page12(ToughVideoCasesPage):

  def __init__(self, page_set):
    super(Page12, self).__init__(
      url='file://tough_video_cases/video.html?src=crowd2160.mp4',
      page_set=page_set, labels=['is_4k', 'is_50fps'])

    self.add_browser_metrics = True

  def RunPageInteractions(self, action_runner):
    self.PlayAction(action_runner)


class Page13(ToughVideoCasesPage):

  def __init__(self, page_set):
    super(Page13, self).__init__(
      url='file://tough_video_cases/video.html?src=tulip2.mp3&type=audio',
      page_set=page_set)

    self.add_browser_metrics = True

  def RunPageInteractions(self, action_runner):
    self.PlayAction(action_runner)


class Page14(ToughVideoCasesPage):

  def __init__(self, page_set):
    super(Page14, self).__init__(
      url='file://tough_video_cases/video.html?src=tulip2.mp4',
      page_set=page_set)

    self.add_browser_metrics = True

  def RunPageInteractions(self, action_runner):
    self.PlayAction(action_runner)


class Page15(ToughVideoCasesPage):

  def __init__(self, page_set):
    super(Page15, self).__init__(
      url='file://tough_video_cases/video.html?src=tulip2.m4a&type=audio',
      page_set=page_set)

    self.add_browser_metrics = True

  def RunPageInteractions(self, action_runner):
    self.PlayAction(action_runner)


class Page16(ToughVideoCasesPage):

  def __init__(self, page_set):
    super(Page16, self).__init__(
      url='file://tough_video_cases/video.html?src=garden2_10s.webm',
      page_set=page_set, labels=['is_4k'])

    self.add_browser_metrics = True

  def RunPageInteractions(self, action_runner):
    self.PlayAction(action_runner)


class Page17(ToughVideoCasesPage):

  def __init__(self, page_set):
    super(Page17, self).__init__(
      url='file://tough_video_cases/video.html?src=garden2_10s.mp4',
      page_set=page_set, labels=['is_4k'])

    self.add_browser_metrics = True

  def RunPageInteractions(self, action_runner):
    self.PlayAction(action_runner)


class Page18(ToughVideoCasesPage):

  def __init__(self, page_set):
    super(Page18, self).__init__(
      url='file://tough_video_cases/video.html?src=garden2_10s.ogv',
      page_set=page_set, labels=['is_4k'])

    self.add_browser_metrics = True

  def RunPageInteractions(self, action_runner):
    self.PlayAction(action_runner)


class Page19(ToughVideoCasesPage):

  def __init__(self, page_set):
    super(Page19, self).__init__(
      url='file://tough_video_cases/video.html?src=tulip2.ogg&type=audio',
      page_set=page_set)

    self.skip_basic_metrics = True

  def RunPageInteractions(self, action_runner):
    self.SeekBeforeAndAfterPlayhead(action_runner)


class Page20(ToughVideoCasesPage):

  def __init__(self, page_set):
    super(Page20, self).__init__(
      url='file://tough_video_cases/video.html?src=tulip2.wav&type=audio',
      page_set=page_set)

    self.skip_basic_metrics = True

  def RunPageInteractions(self, action_runner):
    self.SeekBeforeAndAfterPlayhead(action_runner)


class Page21(ToughVideoCasesPage):

  def __init__(self, page_set):
    super(Page21, self).__init__(
      url='file://tough_video_cases/video.html?src=tulip2.ogv',
      page_set=page_set)

    self.skip_basic_metrics = True

  def RunPageInteractions(self, action_runner):
    self.SeekBeforeAndAfterPlayhead(action_runner)


class Page22(ToughVideoCasesPage):

  def __init__(self, page_set):
    super(Page22, self).__init__(
      url='file://tough_video_cases/video.html?src=tulip2.webm',
      page_set=page_set)

    self.skip_basic_metrics = True

  def RunPageInteractions(self, action_runner):
    self.SeekBeforeAndAfterPlayhead(action_runner)


class Page23(ToughVideoCasesPage):

  def __init__(self, page_set):
    super(Page23, self).__init__(
      url='file://tough_video_cases/video.html?src=tulip2.mp3&type=audio',
      page_set=page_set)

    self.skip_basic_metrics = True

  def RunPageInteractions(self, action_runner):
    self.SeekBeforeAndAfterPlayhead(action_runner)


class Page24(ToughVideoCasesPage):

  def __init__(self, page_set):
    super(Page24, self).__init__(
      url='file://tough_video_cases/video.html?src=tulip2.mp4',
      page_set=page_set)

    self.skip_basic_metrics = True

  def RunPageInteractions(self, action_runner):
    self.SeekBeforeAndAfterPlayhead(action_runner)


class Page25(ToughVideoCasesPage):

  def __init__(self, page_set):
    super(Page25, self).__init__(
      url='file://tough_video_cases/video.html?src=garden2_10s.webm',
      page_set=page_set, labels=['is_4k'])

    self.skip_basic_metrics = True

  def RunPageInteractions(self, action_runner):
    self.SeekBeforeAndAfterPlayhead(action_runner)


class Page26(ToughVideoCasesPage):

  def __init__(self, page_set):
    super(Page26, self).__init__(
      url='file://tough_video_cases/video.html?src=garden2_10s.mp4',
      page_set=page_set, labels=['is_4k'])

    self.skip_basic_metrics = True

  def RunPageInteractions(self, action_runner):
    self.SeekBeforeAndAfterPlayhead(action_runner)


class Page27(ToughVideoCasesPage):

  def __init__(self, page_set):
    super(Page27, self).__init__(
      url='file://tough_video_cases/video.html?src=garden2_10s.ogv',
      page_set=page_set, labels=['is_4k'])

    self.skip_basic_metrics = True

  def RunPageInteractions(self, action_runner):
    self.SeekBeforeAndAfterPlayhead(action_runner)


class Page28(ToughVideoCasesPage):

  def __init__(self, page_set):
    super(Page28, self).__init__(
      url='file://tough_video_cases/audio_playback.html?id=single_audio',
      page_set=page_set)

    self.skip_basic_metrics = True

  def RunPageInteractions(self, action_runner):
    self.LoopSingleAudio(action_runner)


class Page29(ToughVideoCasesPage):

  def __init__(self, page_set):
    super(Page29, self).__init__(
      url='file://tough_video_cases/audio_playback.html?id=mixed_audio',
      page_set=page_set)

    self.skip_basic_metrics = True

  def RunPageInteractions(self, action_runner):
    self.LoopMixedAudio(action_runner)

class Page30(ToughVideoCasesPage):

  def __init__(self, page_set):
    super(Page30, self).__init__(
      url='file://tough_video_cases/video.html?src=tulip2.vp9.webm',
      page_set=page_set)

    self.add_browser_metrics = True

  def RunPageInteractions(self, action_runner):
    self.PlayAction(action_runner)

class Page31(ToughVideoCasesPage):

  def __init__(self, page_set):
    super(Page31, self).__init__(
      url='file://tough_video_cases/video.html?src=tulip2.vp9.webm',
      page_set=page_set)

    self.skip_basic_metrics = True

  def RunPageInteractions(self, action_runner):
    self.SeekBeforeAndAfterPlayhead(action_runner)

class Page32(ToughVideoCasesPage):

  def __init__(self, page_set):
    super(Page32, self).__init__(
      url='file://tough_video_cases/video.html?src=crowd1080_vp9.webm',
      page_set=page_set)

    self.add_browser_metrics = True

  def RunPageInteractions(self, action_runner):
    self.PlayAction(action_runner)

class Page33(ToughVideoCasesPage):

  def __init__(self, page_set):
    super(Page33, self).__init__(
      url='file://tough_video_cases/video.html?src=crowd1080_vp9.webm',
      page_set=page_set)

    self.skip_basic_metrics = True

  def RunPageInteractions(self, action_runner):
    self.SeekBeforeAndAfterPlayhead(action_runner)

class Page34(ToughVideoCasesPage):

  def __init__(self, page_set):
    super(Page34, self).__init__(
      url='file://tough_video_cases/video.html?src=crowd720_vp9.webm',
      page_set=page_set)

    self.add_browser_metrics = True

  def RunPageInteractions(self, action_runner):
    self.PlayAction(action_runner)

class Page35(ToughVideoCasesPage):

  def __init__(self, page_set):
    super(Page35, self).__init__(
      url='file://tough_video_cases/video.html?src=crowd720_vp9.webm',
      page_set=page_set)

    self.skip_basic_metrics = True

  def RunPageInteractions(self, action_runner):
    self.SeekBeforeAndAfterPlayhead(action_runner)

class ToughVideoCasesPageSet(page_set_module.PageSet):

  """
  Description: Video Stack Perf benchmark
  """
  def __init__(self):
    super(ToughVideoCasesPageSet, self).__init__(
            bucket=page_set_module.PARTNER_BUCKET)

    self.AddUserStory(Page1(self))
    self.AddUserStory(Page2(self))
    self.AddUserStory(Page3(self))
    self.AddUserStory(Page4(self))
    self.AddUserStory(Page5(self))
    self.AddUserStory(Page6(self))
    self.AddUserStory(Page7(self))
    self.AddUserStory(Page8(self))
    self.AddUserStory(Page9(self))
    self.AddUserStory(Page10(self))
    self.AddUserStory(Page11(self))
    self.AddUserStory(Page12(self))
    self.AddUserStory(Page13(self))
    self.AddUserStory(Page14(self))
    self.AddUserStory(Page15(self))
    self.AddUserStory(Page16(self))
    self.AddUserStory(Page17(self))
    self.AddUserStory(Page18(self))
    self.AddUserStory(Page19(self))
    self.AddUserStory(Page20(self))
    self.AddUserStory(Page21(self))
    self.AddUserStory(Page22(self))
    self.AddUserStory(Page23(self))
    self.AddUserStory(Page24(self))
    self.AddUserStory(Page25(self))
    self.AddUserStory(Page26(self))
    self.AddUserStory(Page27(self))
    self.AddUserStory(Page28(self))
    self.AddUserStory(Page29(self))
    self.AddUserStory(Page30(self))
    self.AddUserStory(Page31(self))
    self.AddUserStory(Page32(self))
    self.AddUserStory(Page33(self))
    self.AddUserStory(Page34(self))
    self.AddUserStory(Page35(self))
