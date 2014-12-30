# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
from telemetry.page import page as page_module
from telemetry.page import page_set as page_set_module


class ToughScrollingCasesPage(page_module.Page):

  def __init__(self, url, page_set):
    super(ToughScrollingCasesPage, self).__init__(url=url, page_set=page_set)

  def RunPageInteractions(self, action_runner):
    interaction = action_runner.BeginGestureInteraction(
        'ScrollAction', is_smooth=True)
    action_runner.ScrollPage()
    interaction.End()

class ToughFastScrollingCasesPage(page_module.Page):

  def __init__(self, url, name, speed_in_pixels_per_second, page_set):
    super(ToughFastScrollingCasesPage, self).__init__(
      url=url,
      page_set=page_set,
      name=name,
      labels=['fastscrolling'])
    self.speed_in_pixels_per_second = speed_in_pixels_per_second

  def RunPageInteractions(self, action_runner):
    interaction = action_runner.BeginGestureInteraction(
        'ScrollAction', is_smooth=True)
    action_runner.ScrollPage(
      direction='down',
      speed_in_pixels_per_second=self.speed_in_pixels_per_second)
    interaction.End()

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
      self.AddUserStory(ToughScrollingCasesPage(url, self))

    fast_scrolling_page_name_list = [
      'text',
      'canvas'
    ]

    fast_scrolling_speed_list = [
      5000, 10000, 15000, 20000, 30000, 40000, 50000, 60000, 75000, 90000
    ]

    for name in fast_scrolling_page_name_list:
      for speed in fast_scrolling_speed_list:
        self.AddUserStory(ToughFastScrollingCasesPage(
          'file://tough_scrolling_cases/' + name + '.html',
          name + '_' + str(speed).zfill(5) + '_pixels_per_second',
          speed,
          self))
