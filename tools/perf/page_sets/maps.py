# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
# pylint: disable=W0401,W0614
from telemetry.page.actions.all_page_actions import *
from telemetry.page import page as page_module
from telemetry.page import page_set as page_set_module


class MapsPage(page_module.PageWithDefaultRunNavigate):

  def __init__(self, page_set):
    super(MapsPage, self).__init__(
      url='http://localhost:10020/tracker.html',
      page_set=page_set)
    self.archive_data_file = 'data/maps.json'
    self.name = 'Maps.maps_001'

  def RunNavigateSteps(self, action_runner):
    action_runner.RunAction(NavigateAction())
    action_runner.RunAction(WaitAction({'seconds': 3}))

  def RunSmoothness(self, action_runner):
    action_runner.RunAction(WaitAction({'javascript': 'window.testDone'}))


class MapsPageSet(page_set_module.PageSet):

  """ Google Maps examples """

  def __init__(self):
    super(MapsPageSet, self).__init__(
        archive_data_file='data/maps.json')

    self.AddPage(MapsPage(self))
