# Copyright 2016 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
from telemetry.page import page as page_module
from telemetry.page import shared_page_state
from telemetry import story


class _NoGpuSharedPageState(shared_page_state.SharedPageState):
  def __init__(self, test, finder_options, story_set):
    super(_NoGpuSharedPageState, self).__init__(
      test, finder_options, story_set)
    finder_options.browser_options.AppendExtraBrowserArgs(
      ['--disable-gpu'])


class _NoOverlaysSharedPageState(shared_page_state.SharedPageState):
  def __init__(self, test, finder_options, story_set):
    super(_NoOverlaysSharedPageState, self).__init__(
      test, finder_options, story_set)
    finder_options.browser_options.AppendExtraBrowserArgs(
      ['--disable-mac-overlays'])


class TrivialScrollingPage(page_module.Page):

  def __init__(self, page_set, shared_page_state_class):
    super(TrivialScrollingPage, self).__init__(
        url='file://trivial_sites/trivial_scrolling_page.html',
        page_set=page_set,
        name=self.__class__.__name__ + shared_page_state_class.__name__,
        shared_page_state_class=shared_page_state_class)


class TrivialBlinkingCursorPage(page_module.Page):

  def __init__(self, page_set, shared_page_state_class):
    super(TrivialBlinkingCursorPage, self).__init__(
        url='file://trivial_sites/trivial_blinking_cursor.html',
        page_set=page_set,
        name=self.__class__.__name__ + shared_page_state_class.__name__,
        shared_page_state_class=shared_page_state_class)


class MacGpuTrivialPagesStorySet(story.StorySet):

  def __init__(self):
    super(MacGpuTrivialPagesStorySet, self).__init__()
    self.AddStory(TrivialScrollingPage(self, shared_page_state.SharedPageState))
    self.AddStory(TrivialBlinkingCursorPage(
        self, shared_page_state.SharedPageState))
    self.AddStory(TrivialScrollingPage(self, _NoOverlaysSharedPageState))
    self.AddStory(TrivialBlinkingCursorPage(self, _NoOverlaysSharedPageState))
    self.AddStory(TrivialScrollingPage(self, _NoGpuSharedPageState))
    self.AddStory(TrivialBlinkingCursorPage(self, _NoGpuSharedPageState))

  @property
  def allow_mixed_story_states(self):
    # Return True here in order to be able to add the same tests with
    # a different SharedPageState.
    return True
