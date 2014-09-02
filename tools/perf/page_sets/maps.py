# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import logging

from telemetry.page import page as page_module
from telemetry.page import page_set as page_set_module


class MapsPage(page_module.Page):

  def __init__(self, page_set):
    super(MapsPage, self).__init__(
      url='http://localhost:10020/tracker.html',
      page_set=page_set,
      name='Maps.maps_002')
    self.archive_data_file = 'data/maps.json'

  def CanRunOnBrowser(self, browser_info):
    if not browser_info.HasWebGLSupport():
      logging.warning('Browser does not support webgl, skipping test')
      return False
    return True

  def RunNavigateSteps(self, action_runner):
    action_runner.NavigateToPage(self)
    action_runner.Wait(3)

  def RunSmoothness(self, action_runner):
    action_runner.WaitForJavaScriptCondition('window.testDone', 120)


class MapsPageSet(page_set_module.PageSet):

  """ Google Maps examples """

  def __init__(self):
    super(MapsPageSet, self).__init__(
        archive_data_file='data/maps.json',
        bucket=page_set_module.PUBLIC_BUCKET)

    self.AddPage(MapsPage(self))
