# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
from telemetry.page import page as page_module
from telemetry import story


class BlankPage(page_module.Page):
  def __init__(self, url, page_set):
    super(BlankPage, self).__init__(url, page_set=page_set)


class BlankPageSet(story.StorySet):
  """A single blank page."""

  def __init__(self):
    super(BlankPageSet, self).__init__()
    self.AddStory(BlankPage('file://blank_page/blank_page.html', self))
