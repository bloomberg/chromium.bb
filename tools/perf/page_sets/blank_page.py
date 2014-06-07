# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
from telemetry.page import page as page_module
from telemetry.page import page_set as page_set_module


class BlankPage(page_module.Page):
  def __init__(self, url, page_set):
    super(BlankPage, self).__init__(url, page_set=page_set)


class BlankPageSet(page_set_module.PageSet):
  """A single blank page."""

  def __init__(self):
    super(BlankPageSet, self).__init__()
    self.AddPage(BlankPage('file://blank_page/blank_page.html', self))
