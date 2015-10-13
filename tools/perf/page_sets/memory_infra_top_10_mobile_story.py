# Copyright 2015 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import logging

from page_sets import top_10_mobile
from telemetry import story


DUMP_WAIT_TIME = 3


class MemoryInfraTop10MobilePage(top_10_mobile.Top10MobilePage):

  def RunPageInteractions(self, action_runner):
    action_runner.tab.WaitForDocumentReadyStateToBeComplete()
    action_runner.Wait(1)  # See crbug.com/540022#c17.
    with action_runner.CreateInteraction('foreground'):
      action_runner.Wait(DUMP_WAIT_TIME)
      action_runner.ForceGarbageCollection()
      action_runner.Wait(DUMP_WAIT_TIME)
      if not action_runner.tab.browser.DumpMemory():
        logging.error('Unable to get a memory dump for %s.', self.name)


class MemoryInfraTop10MobilePageSet(story.StorySet):

  """ Top 10 mobile sites for memory measurements """

  def __init__(self):
    super(MemoryInfraTop10MobilePageSet, self).__init__(
      archive_data_file='data/top_10_mobile.json',
      cloud_storage_bucket=story.PARTNER_BUCKET)

    for url in top_10_mobile.URL_LIST:
      self.AddStory(MemoryInfraTop10MobilePage(url, self, False))
