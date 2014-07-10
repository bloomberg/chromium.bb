# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
from telemetry.page import page as page_module
from telemetry.page import page_set as page_set_module

class ToughPepperCasesPage(page_module.Page):

  def __init__(self, url, page_set):
    super(ToughPepperCasesPage, self).__init__(url=url, page_set=page_set)

  def RunSmoothness(self, action_runner):
    interaction = action_runner.BeginGestureInteraction(
        'ScrollAction', is_smooth=True)
    action_runner.ScrollPage()
    interaction.End()


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
    interaction = action_runner.BeginGestureInteraction(
        'ScrollAction', is_smooth=True)
    action_runner.ScrollPage(
        use_touch=True,
        direction='up',
        top_start_ratio=0.3,
        left_start_ratio=0.3,
        speed_in_pixels_per_second=200,
        distance=500)
    interaction.End()

class ToughPepperCasesPageSet(page_set_module.PageSet):

  """ Pepper latency test cases """

  def __init__(self):
    super(ToughPepperCasesPageSet, self).__init__()

    self.AddPage(Page1(self))
