# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from telemetry.page import page as page_module
from telemetry.page import page_set as page_set_module


class SafebrowsingPage(page_module.Page):

  """
  Why: Expect 'malware ahead' page. Use a short navigation timeout because no
  response will be received.
  """

  def __init__(self, url, page_set):
    super(SafebrowsingPage, self).__init__(url=url, page_set=page_set)
    self.archive_data_file = '../data/chrome_proxy_safebrowsing.json'

  def RunNavigateSteps(self, action_runner):
    action_runner.NavigateToPage(self, timeout_in_seconds=5)


class SafebrowsingPageSet(page_set_module.PageSet):

  """ Chrome proxy test sites """

  def __init__(self):
    super(SafebrowsingPageSet, self).__init__(
      archive_data_file='../data/chrome_proxy_safebrowsing.json')

    self.AddPage(SafebrowsingPage('http://www.ianfette.org/', self))
