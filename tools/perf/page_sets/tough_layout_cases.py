# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
from telemetry.page import page as page_module
from telemetry.page import page_set as page_set_module


class ToughLayoutCasesPage(page_module.Page):

  def __init__(self, url, page_set):
    super(ToughLayoutCasesPage, self).__init__(url=url, page_set=page_set)
    self.credentials_path = 'data/credentials.json'
    self.user_agent_type = 'desktop'
    self.archive_data_file = 'data/tough_layout_cases.json'

  def RunSmoothness(self, action_runner):
    interaction = action_runner.BeginGestureInteraction(
        'ScrollAction', is_smooth=True)
    action_runner.ScrollPage()
    interaction.End()


class ToughLayoutCasesPageSet(page_set_module.PageSet):

  """
  The slowest layouts observed in the alexa top 1 million sites in  July 2013.
  """

  def __init__(self):
    super(ToughLayoutCasesPageSet, self).__init__(
      credentials_path='data/credentials.json',
      user_agent_type='desktop',
      archive_data_file='data/tough_layout_cases.json',
      bucket=page_set_module.PARTNER_BUCKET)

    urls_list = [
      'http://oilevent.com',
      'http://www.muzoboss.ru',
      'http://natunkantha.com',
      'http://www.mossiella.com',
      'http://bookish.com',
      'http://mydiyclub.com',
      'http://amarchoti.blogspot.com',
      'http://picarisimo.es',
      'http://chinaapache.com',
      'http://indoritel.com'
    ]

    for url in urls_list:
      self.AddPage(ToughLayoutCasesPage(url, self))
