# Copyright 2016 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from page_sets import trivial_sites
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


class _NoWebGLImageChromiumSharedPageState(shared_page_state.SharedPageState):
  def __init__(self, test, finder_options, story_set):
    super(_NoWebGLImageChromiumSharedPageState, self).__init__(
      test, finder_options, story_set)
    finder_options.browser_options.AppendExtraBrowserArgs(
      ['--disable-webgl-image-chromium'])


class MacGpuTrivialPagesStorySet(story.StorySet):

  def __init__(self, wait_in_seconds=0):
    # Wait is time to wait_in_seconds on page in seconds.
    super(MacGpuTrivialPagesStorySet, self).__init__(
        cloud_storage_bucket=story.PUBLIC_BUCKET)

    default_trivial_sites = trivial_sites.TrivialSitesStorySet(
        wait_in_seconds = wait_in_seconds)
    for user_story in default_trivial_sites.stories:
      self.AddStory(user_story)

    no_overlays_trivial_sites = trivial_sites.TrivialSitesStorySet(
        _NoOverlaysSharedPageState, wait_in_seconds)
    for user_story in no_overlays_trivial_sites.stories:
      self.AddStory(user_story)

    self.AddStory(trivial_sites.TrivialScrollingPage(
        self, _NoGpuSharedPageState, wait_in_seconds))
    self.AddStory(trivial_sites.TrivialBlinkingCursorPage(
        self, _NoGpuSharedPageState, wait_in_seconds))
    self.AddStory(trivial_sites.TrivialCanvasPage(
        self, _NoGpuSharedPageState, wait_in_seconds))
    self.AddStory(trivial_sites.TrivialWebGLPage(
        self, _NoWebGLImageChromiumSharedPageState, wait_in_seconds))
    self.AddStory(trivial_sites.TrivialBlurAnimationPage(
        self, _NoGpuSharedPageState, wait_in_seconds))
    self.AddStory(trivial_sites.TrivialFullscreenVideoPage(
        self, _NoGpuSharedPageState, wait_in_seconds))
    self.AddStory(trivial_sites.TrivialGifPage(
        self, _NoGpuSharedPageState, wait_in_seconds))

  @property
  def allow_mixed_story_states(self):
    # Return True here in order to be able to add the same tests with
    # a different SharedPageState.
    return True
