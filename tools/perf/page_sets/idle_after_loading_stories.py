# Copyright 2016 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
from telemetry.page import page as page_module
from telemetry import story


# TODO(rnephew): Move to seperate file and merge with mac_gpu_sites BasePage.
class _BasePage(page_module.Page):
  def __init__(self, page_set, url, wait_in_seconds):
    super(_BasePage, self).__init__(url=url, page_set=page_set)
    self._wait_in_seconds = wait_in_seconds

  def RunPageInteractions(self, action_runner):
    action_runner.Wait(self._wait_in_seconds)


class IdleAfterLoadingStories(story.StorySet):
  """Historically, Chrome has high CPU usage on these sites after the page has
  loaded. These user stories let Chrome idle on the page."""

  def __init__(self, wait_in_seconds=0):
    super(IdleAfterLoadingStories, self).__init__(
        archive_data_file='data/idle_after_loading_stories.json',
        cloud_storage_bucket=story.PARTNER_BUCKET)

    # Chrome has high idle CPU usage on this site, even after its quiesced.
    # https://crbug.com/638365.
    urls = [
        'http://www.labradortraininghq.com/labrador-training/how-to-crate-train'
        '-a-puppy/#How_Long_DoesIt_Take_To_Crate_Train_A_Puppy'
    ]
    for url in urls:
      self.AddStory(_BasePage(self, url, wait_in_seconds))
