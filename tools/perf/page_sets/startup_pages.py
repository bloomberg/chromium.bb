# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
from telemetry.page import page as page_module
from telemetry.page import page_set as page_set_module


class StartedPage(page_module.Page):

  def __init__(self, url, startup_url, page_set):
    super(StartedPage, self).__init__(url=url, page_set=page_set)
    self.archive_data_file = 'data/startup_pages.json'
    self.startup_url = startup_url

  def RunNavigateSteps(self, action_runner):
    action_runner.Wait(10)

class StartupPagesPageSet(page_set_module.PageSet):

  """ Pages for testing starting Chrome with a URL.
  Note that this file can't be used with record_wpr, since record_wpr requires
  a true navigate step, which we do not want for startup testing. Instead use
  record_wpr startup_pages_record to record data for this test.
  """

  def __init__(self):
    super(StartupPagesPageSet, self).__init__(
        archive_data_file='data/startup_pages.json',
        bucket=page_set_module.PARTNER_BUCKET)

    # Typical page.
    self.AddPage(StartedPage('about:blank', 'about:blank', self))
    # Typical page.
    self.AddPage(StartedPage('http://bbc.co.uk', 'http://bbc.co.uk', self))
    # Horribly complex page - stress test!
    self.AddPage(StartedPage('http://kapook.com', 'http://kapook.com', self))
