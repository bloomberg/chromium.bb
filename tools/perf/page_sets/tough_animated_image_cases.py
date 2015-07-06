# Copyright 2015 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
from telemetry.page import page as page_module
from telemetry import story

class ToughAnimatedImageCasesPage(page_module.Page):

  def __init__(self, url, page_set):
    super(ToughAnimatedImageCasesPage, self).__init__(
        url=url, page_set=page_set)

  def RunPageInteractions(self, action_runner):
    with action_runner.CreateInteraction('ToughAnimatedImage'):
      action_runner.Wait(10)

class ToughAnimatedImageCasesPageSet(story.StorySet):

  """
  Description: A collection of difficult animated image tests
  """

  def __init__(self):
    super(ToughAnimatedImageCasesPageSet, self).__init__()

    urls_list = [
      'file://tough_animated_image_cases/gifs.html'
    ]

    for url in urls_list:
      self.AddStory(ToughAnimatedImageCasesPage(url, self))
