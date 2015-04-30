# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
from telemetry.page import page as page_module
from telemetry.page import page_set as page_set_module


class ToughFastScrollingCasesPage(page_module.Page):

  def __init__(self, url, name, speed_in_pixels_per_second, page_set):
    super(ToughFastScrollingCasesPage, self).__init__(
      url=url,
      page_set=page_set,
      name=name)
    self.speed_in_pixels_per_second = speed_in_pixels_per_second

  def RunPageInteractions(self, action_runner):
    with action_runner.CreateGestureInteraction('ScrollAction'):
      action_runner.ScrollPage(
          direction='down',
          speed_in_pixels_per_second=self.speed_in_pixels_per_second)

class ToughScrollingCasesPageSet(page_set_module.PageSet):

  """
  Description: A collection of difficult scrolling tests
  """

  def __init__(self):
    super(ToughScrollingCasesPageSet, self).__init__()

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
