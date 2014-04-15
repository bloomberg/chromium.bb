# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
# pylint: disable=W0401,W0614
from telemetry.page.actions.all_page_actions import *
from telemetry.page import page as page_module
from telemetry.page import page_set as page_set_module


class ToughLayoutCasesPage(page_module.PageWithDefaultRunNavigate):

  def __init__(self, url, page_set):
    super(ToughLayoutCasesPage, self).__init__(url=url, page_set=page_set)
    self.credentials_path = 'data/credentials.json'
    self.user_agent_type = 'desktop'
    self.archive_data_file = 'data/tough_layout_cases.json'

  def RunSmoothness(self, action_runner):
    action_runner.RunAction(ScrollAction())


class ToughLayoutCasesPageSet(page_set_module.PageSet):

  """
  The slowest layouts observed in the alexa top 1 million sites in  July 2013.
  """

  def __init__(self):
    super(ToughLayoutCasesPageSet, self).__init__(
      credentials_path='data/credentials.json',
      user_agent_type='desktop',
      archive_data_file='data/tough_layout_cases.json')

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
