# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
from telemetry.page import page as page_module
from telemetry.page import shared_page_state
from telemetry import story

class BigJsPageSet(story.StorySet):

  """ Sites which load and run big JavaScript files."""

  def __init__(self):
    super(BigJsPageSet, self).__init__(
      archive_data_file='data/big_js.json',
      cloud_storage_bucket=story.PARTNER_BUCKET)

    # www.foo.com is a dummy page to make it easier to run only one interesting
    # page at a time. You can't just run the interesting page on its own: Page
    # sets with only one page don't work well, since we end up reusing a
    # renderer all the time and it keeps its memory caches alive (see
    # crbug.com/403735).
    urls_list = [
       'http://beta.unity3d.com/jonas/DT2/',
       'https://www.youtube.com/watch?v=IJNR2EpS0jw',
       'http://www.foo.com',
    ]
    for url in urls_list:
      self.AddStory(page_module.Page(
          url, self,
          shared_page_state_class=shared_page_state.SharedDesktopPageState))
