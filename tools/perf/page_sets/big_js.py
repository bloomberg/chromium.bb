# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
from telemetry.page import page as page_module
from telemetry.page import page_set as page_set_module

class BigJsPageSet(page_set_module.PageSet):

  """ Sites which load and run big JavaScript files."""

  def __init__(self):
    super(BigJsPageSet, self).__init__(
      archive_data_file='data/big_js.json',
      bucket=page_set_module.PARTNER_BUCKET,
      user_agent_type='desktop')

    # Page sets with only one page don't work well, since we end up reusing a
    # renderer all the time and it keeps its memory caches alive (see
    # crbug.com/403735). Add a dummy second page here.
    urls_list = [
        'http://beta.unity3d.com/jonas/DT2/',
        'http://www.foo.com',
    ]
    for url in urls_list:
      self.AddUserStory(page_module.Page(url, self))
