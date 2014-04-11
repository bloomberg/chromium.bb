# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
# pylint: disable=W0401,W0614
from telemetry.page.actions.all_page_actions import *
from telemetry.page import page as page_module
from telemetry.page import page_set as page_set_module

class MorejsPage(page_module.PageWithDefaultRunNavigate):

  def __init__(self, url, page_set):
    super(MorejsPage, self).__init__(url=url, page_set=page_set)


class MorejsPageSet(page_set_module.PageSet):

  """ More JS page_cycler benchmark """

  def __init__(self):
    super(MorejsPageSet, self).__init__(
      # pylint: disable=C0301
      serving_dirs=set(['../../../../data/page_cycler/morejs']))

    urls_list = [
      'file://../../../../data/page_cycler/morejs/blog.chromium.org/',
      'file://../../../../data/page_cycler/morejs/dev.chromium.org/',
      'file://../../../../data/page_cycler/morejs/googleblog.blogspot.com1/',
      'file://../../../../data/page_cycler/morejs/googleblog.blogspot.com2/',
      'file://../../../../data/page_cycler/morejs/test.blogspot.com/',
      'file://../../../../data/page_cycler/morejs/www.igoogle.com/',
      'file://../../../../data/page_cycler/morejs/www.techcrunch.com/',
      'file://../../../../data/page_cycler/morejs/www.webkit.org/',
      'file://../../../../data/page_cycler/morejs/www.yahoo.com/'
    ]

    for url in urls_list:
      self.AddPage(MorejsPage(url, self))
