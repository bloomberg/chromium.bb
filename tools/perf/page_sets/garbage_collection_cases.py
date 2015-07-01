# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
# pylint: disable=W0401,W0614
from telemetry.page import page as page_module
from telemetry import story


class SpinningBallsPage(page_module.Page):

  def __init__(self, page_set):
    super(SpinningBallsPage, self).__init__(
      # pylint: disable=C0301
      url='http://v8.googlecode.com/svn/branches/bleeding_edge/benchmarks/spinning-balls/index.html',
      page_set=page_set)

  def RunNavigateSteps(self, action_runner):
    super(SpinningBallsPage, self).RunNavigateSteps(action_runner)
    action_runner.WaitForJavaScriptCondition(
        "document.readyState == 'complete'")
    action_runner.ClickElement(selector='input[type="submit"]')
    action_runner.WaitForJavaScriptCondition(
        "document.readyState == 'complete'")

  def RunPageInteractions(self, action_runner):
    with action_runner.CreateInteraction('RunSmoothAllActions'):
      action_runner.Wait(15)


class GarbageCollectionCasesPageSet(story.StorySet):

  """
  Description: GC test cases
  """

  def __init__(self):
    super(GarbageCollectionCasesPageSet, self).__init__(
      archive_data_file='data/garbage_collection_cases.json',
      cloud_storage_bucket=story.PARTNER_BUCKET)

    self.AddUserStory(SpinningBallsPage(self))
