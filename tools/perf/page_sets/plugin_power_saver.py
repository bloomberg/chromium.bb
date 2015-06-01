# Copyright 2015 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from telemetry.page import page
from telemetry.page import page_set


class PluginPowerSaverPageSet(page_set.PageSet):
  def __init__(self):
    super(PluginPowerSaverPageSet, self).__init__(
        user_agent_type='desktop',
        archive_data_file='data/plugin_power_saver.json',
        bucket=page_set.PUBLIC_BUCKET)
    self.AddUserStory(page.Page('http://a.tommycli.com/small_only.html',
                                page_set=self))
