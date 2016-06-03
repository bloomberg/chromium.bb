# Copyright 2016 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
from telemetry.page import page as page_module
from telemetry import story


class TrivialScrollingPage(page_module.Page):

  def __init__(self, page_set):
    super(TrivialScrollingPage, self).__init__(
        url='file://trivial_sites/trivial_scrolling_page.html',
        page_set=page_set)


class TrivialScrollStorySet(story.StorySet):

  def __init__(self):
    super(TrivialScrollStorySet, self).__init__()
    self.AddStory(TrivialScrollingPage(self))
