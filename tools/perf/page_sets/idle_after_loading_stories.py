# Copyright 2016 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
from telemetry.page import page as page_module
from telemetry import story


class IdleAfterLoadingStories(story.StorySet):
  """Historically, Chrome has high CPU usage on these sites after the page has
  loaded. These user stories let Chrome idle on the page."""

  def __init__(self):
    super(IdleAfterLoadingStories, self).__init__(
        archive_data_file='data/idle_after_loading_stories.json',
        cloud_storage_bucket=story.PARTNER_BUCKET)

    # Chrome has high idle CPU usage on this site, even after its quiesced.
    # https://crbug.com/638365.
    self.AddStory(page_module.Page(
        'http://www.labradortraininghq.com/labrador-training/how-to-crate-train'
        '-a-puppy/#How_Long_DoesIt_Take_To_Crate_Train_A_Puppy', page_set=self))
