# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
from telemetry.page import page as page_module
from telemetry import story


class ToughTextureUploadCasesPage(page_module.Page):

  def __init__(self, name, url, page_set):
    super(
      ToughTextureUploadCasesPage,
      self).__init__(
        url=url,
        page_set=page_set,
        name=name)

  def RunPageInteractions(self, action_runner):
    with action_runner.CreateInteraction('Animation'):
      action_runner.Wait(10)


class ToughTextureUploadCasesPageSet(story.StorySet):

  """
  Description: A collection of texture upload performance tests
  """

  def __init__(self):
    super(ToughTextureUploadCasesPageSet, self).__init__()

    urls_list = [
      ('background_color_animation',
       'file://tough_texture_upload_cases/background_color_animation.html'),
      ('background_color_animation_with_gradient',
       # pylint: disable=line-too-long
       'file://tough_texture_upload_cases/background_color_animation_with_gradient.html'),
      ('small_texture_uploads',
       'file://tough_texture_upload_cases/small_texture_uploads.html'),
      ('medium_texture_uploads',
       'file://tough_texture_upload_cases/medium_texture_uploads.html'),
      ('large_texture_uploads',
       'file://tough_texture_upload_cases/large_texture_uploads.html'),
      ('extra_large_texture_uploads',
       'file://tough_texture_upload_cases/extra_large_texture_uploads.html'),
    ]
    for name, url in urls_list:
      self.AddStory(ToughTextureUploadCasesPage(name=name,
                                                url=url,
                                                page_set=self))
