# Copyright 2015 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
import time

from telemetry.page import page as page_module
from telemetry.page import shared_page_state
from telemetry import story

INTERACTION_NAME = 'Interaction.PageLoading'

# How long to wait for the page to finish rendering.
LOADING_DELAY_S = 5


class NewTabPagePage(page_module.Page):

  def __init__(self, page_set):
    super(NewTabPagePage, self).__init__(
      name='newtabpagepage',
      url='chrome://newtab',
      shared_page_state_class=shared_page_state.SharedDesktopPageState,
      page_set=page_set)
    self.archive_data_file = 'data/new_tab_page_page.json'
    self.script_to_evaluate_on_commit = (
        "console.time('" + INTERACTION_NAME + "');")

  def RunNavigateSteps(self, action_runner):
    url = self.file_path_url_with_scheme if self.is_file else self.url
    action_runner.Navigate(
        url, script_to_evaluate_on_commit=self.script_to_evaluate_on_commit)
    # We pause for a while so the async JS gets a chance to run.
    time.sleep(LOADING_DELAY_S)
    # TODO(beaudoin): We have no guarantee the page has fully rendered and the
    #   JS has fully executed at that point. The right way to do it would be to
    #   toggle a Javascript variable in the page code and to wait for it here.
    #   This should be done when window.performance.mark is supported and we
    #   migrate to it instead of calling console.timeEnd here. If the test is
    #   failing flakily, we should use a better heuristic.
    action_runner.ExecuteJavaScript(
        "console.timeEnd('" + INTERACTION_NAME + "');")


class NewTabPagePageSet(story.StorySet):
  def __init__(self):
    super(NewTabPagePageSet, self).__init__(
        archive_data_file='data/new_tab_page_page.json',
        cloud_storage_bucket=story.PUBLIC_BUCKET)
    self.AddStory(NewTabPagePage(page_set=self))
