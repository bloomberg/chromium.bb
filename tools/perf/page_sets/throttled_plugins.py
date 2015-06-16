# Copyright 2015 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
from telemetry.page import shared_page_state
from telemetry.page import page
from telemetry.page import page_set


class ThrottledPluginsPageSet(page_set.PageSet):
  def __init__(self):
    super(ThrottledPluginsPageSet, self).__init__(
        archive_data_file='data/throttled_plugins.json',
        bucket=page_set.PUBLIC_BUCKET)
    urls_with_throttled_plugins = [
        'http://cnn.com',
        'http://techcrunch.com',
        'http://wsj.com',
        'http://youtube.com'
    ]
    for url in urls_with_throttled_plugins:
      self.AddUserStory(
          page.Page(
              url, page_set=self,
              shared_page_state_class=shared_page_state.SharedDesktopPageState))
