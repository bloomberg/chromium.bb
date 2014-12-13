# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from page_sets import top_desktop_sites_2012Q3

from telemetry.page import page_set
from telemetry.page import page


class Top2012Q3StressPage(page.Page):

  def __init__(self, url, ps):
    super(Top2012Q3StressPage, self).__init__(
        url=url, page_set=ps, credentials_path='data/credentials.json')
    self.make_javascript_deterministic = True
    self.archive_data_file = 'data/2012Q3.json'

  def RunPageInteractions(self, action_runner):
    for _ in xrange(3):
      action_runner.ReloadPage()
      action_runner.Wait(1)
      action_runner.ForceGarbageCollection()


class Top2012Q3StressPageSet(page_set.PageSet):
  """ Pages hand-picked from top-lists in Q32012. """

  def __init__(self):
    super(Top2012Q3StressPageSet, self).__init__(
      make_javascript_deterministic=True,
      archive_data_file='data/2012Q3.json',
      bucket=page_set.PARTNER_BUCKET)

    for url in top_desktop_sites_2012Q3.TOP_2013_URLS:
      self.AddUserStory(Top2012Q3StressPage(url, self))
