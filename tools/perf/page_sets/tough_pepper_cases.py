# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
# pylint: disable=W0401,W0614
from telemetry.page.actions.all_page_actions import *
from telemetry.page import page as page_module
from telemetry.page import page_set as page_set_module

class ToughPepperCasesPage(page_module.Page):

  def __init__(self, url, page_set):
    super(ToughPepperCasesPage, self).__init__(url=url, page_set=page_set)

  def RunSmoothness(self, action_runner):
    action_runner.RunAction(ScrollAction())


class Page1(ToughPepperCasesPage):

  """ Why: Simple pepper plugin for touch drawing """

  def __init__(self, page_set):
    super(Page1, self).__init__(
        url='file://tough_pepper_cases/simple_pepper_plugin.html',
        page_set=page_set)

  def RunSmoothness(self, action_runner):
    # Wait until the page and the plugin module are loaded.
    action_runner.WaitForJavaScriptCondition(
        'pageLoaded === true && moduleLoaded === true')
    action_runner.RunAction(ScrollAction(
        {
            'scroll_requires_touch': True,
            'direction': 'up',
            'top_start_percentage': 0.3,
            'left_start_percentage': 0.3,
            'speed': 200,
            'scroll_distance_function': 'function() { return 500; }',
        }))

class ToughPepperCasesPageSet(page_set_module.PageSet):

  """ Pepper latency test cases """

  def __init__(self):
    super(ToughPepperCasesPageSet, self).__init__()

    self.AddPage(Page1(self))
