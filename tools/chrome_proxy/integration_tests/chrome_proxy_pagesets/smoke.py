# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from telemetry.page import page as page_module
from telemetry.page import page_set as page_set_module


class SmokePage(page_module.Page):

  def __init__(self, url, page_set, name=''):
    super(SmokePage, self).__init__(url=url, page_set=page_set, name=name)
    self.archive_data_file = '../data/chrome_proxy_smoke.json'


class Page1(SmokePage):

  """
  Why: Check chrome proxy response headers.
  """

  def __init__(self, page_set):
    super(Page1, self).__init__(
      url='http://aws1.mdw.la/fw/',
      page_set=page_set,
      name='header validation')


class Page2(SmokePage):

  """
  Why: Check data compression
  """

  def __init__(self, page_set):
    super(Page2, self).__init__(
      url='http://aws1.mdw.la/static/',
      page_set=page_set,
      name='compression: image')


class Page3(SmokePage):

  """
  Why: Check bypass
  """

  def __init__(self, page_set):
    super(Page3, self).__init__(
      url='http://aws1.mdw.la/bypass/',
      page_set=page_set,
      name='bypass')


class Page4(SmokePage):

  """
  Why: Check data compression
  """

  def __init__(self, page_set):
    super(Page4, self).__init__(
      url='http://aws1.mdw.la/static/',
      page_set=page_set,
      name='compression: javascript')


class Page5(SmokePage):

  """
  Why: Check data compression
  """

  def __init__(self, page_set):
    super(Page5, self).__init__(
      url='http://aws1.mdw.la/static/',
      page_set=page_set,
      name='compression: css')



class SmokePageSet(page_set_module.PageSet):

  """ Chrome proxy test sites """

  def __init__(self):
    super(SmokePageSet, self).__init__(
      archive_data_file='../data/chrome_proxy_smoke.json')

    self.AddUserStory(Page1(self))
    self.AddUserStory(Page2(self))
    self.AddUserStory(Page3(self))
    self.AddUserStory(Page4(self))
    self.AddUserStory(Page5(self))
