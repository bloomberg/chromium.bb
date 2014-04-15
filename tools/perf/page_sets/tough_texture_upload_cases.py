# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
# pylint: disable=W0401,W0614
from telemetry.page.actions.all_page_actions import *
from telemetry.page import page as page_module
from telemetry.page import page_set as page_set_module


class ToughTextureUploadCasesPage(page_module.PageWithDefaultRunNavigate):

  def __init__(self, url, page_set):
    super(
      ToughTextureUploadCasesPage,
      self).__init__(
        url=url,
        page_set=page_set)

  def RunSmoothness(self, action_runner):
    action_runner.RunAction(ScrollAction())


class ToughTextureUploadCasesPageSet(page_set_module.PageSet):

  """
  Description: A collection of texture upload performance tests
  """

  def __init__(self):
    super(ToughTextureUploadCasesPageSet, self).__init__()

    urls_list = [
      'file://tough_texture_upload_cases/background_color_animation.html',
      # pylint: disable=C0301
      'file://tough_texture_upload_cases/background_color_animation_and_transform_animation.html',
      # pylint: disable=C0301
      'file://tough_texture_upload_cases/background_color_animation_with_gradient.html',
      # pylint: disable=C0301
      'file://tough_texture_upload_cases/background_color_animation_with_gradient_and_transform_animation.html']
    for url in urls_list:
      self.AddPage(ToughTextureUploadCasesPage(url, self))

