# Copyright 2015 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
from telemetry.page import page
from telemetry.page import shared_page_state
from telemetry import story


class PluginPowerSaverPageSet(story.StorySet):
  def __init__(self):
    super(PluginPowerSaverPageSet, self).__init__(
        archive_data_file='data/plugin_power_saver.json',
        cloud_storage_bucket=story.PUBLIC_BUCKET)
    self.AddStory(page.Page(
        'http://a.tommycli.com/small_only.html',
        page_set=self,
        shared_page_state_class=shared_page_state.SharedDesktopPageState))
