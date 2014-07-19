# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from telemetry.page import page as page_module
from telemetry.page import page_set as page_set_module


class FallbackViaHeaderPage(page_module.Page):

  def __init__(self, url, page_set):
    super(FallbackViaHeaderPage, self).__init__(url=url, page_set=page_set)


class FallbackViaHeaderPageSet(page_set_module.PageSet):

  """ Chrome proxy test sites """

  def __init__(self):
    super(FallbackViaHeaderPageSet, self).__init__()

    self.AddPage(FallbackViaHeaderPage(
        'http://chromeproxy-test.appspot.com/default', self))
