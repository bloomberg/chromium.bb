# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
# pylint: disable=W0401,W0614
from telemetry.page.actions.all_page_actions import *
from telemetry.page import page as page_module
from telemetry.page import page_set as page_set_module


class ToughScrollingCasesPage(page_module.PageWithDefaultRunNavigate):

  def __init__(self, url, page_set):
    super(ToughScrollingCasesPage, self).__init__(url=url, page_set=page_set)

  def RunSmoothness(self, action_runner):
    action_runner.RunAction(ScrollAction())


class ToughScrollingCasesPageSet(page_set_module.PageSet):

  """
  Description: A collection of difficult scrolling tests
  """

  def __init__(self):
    super(ToughScrollingCasesPageSet, self).__init__()

    urls_list = [
      'file://tough_scrolling_cases/background_fixed.html',
      'file://tough_scrolling_cases/cust_scrollbar.html',
      'file://tough_scrolling_cases/div_scrolls.html',
      'file://tough_scrolling_cases/fixed_nonstacking.html',
      'file://tough_scrolling_cases/fixed_stacking.html',
      'file://tough_scrolling_cases/iframe_scrolls.html',
      'file://tough_scrolling_cases/simple.html',
      'file://tough_scrolling_cases/wheel_body_prevdefault.html',
      'file://tough_scrolling_cases/wheel_div_prevdefault.html'
    ]

    for url in urls_list:
      self.AddPage(ToughScrollingCasesPage(url, self))

