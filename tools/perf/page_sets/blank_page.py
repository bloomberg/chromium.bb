# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from telemetry.page.actions.navigate import NavigateAction
from telemetry.page.page_set import PageSet
from telemetry.page.page import PageWithDefaultRunNavigate


class BlankPage(PageWithDefaultRunNavigate):
  def __init__(self, url, page_set):
    super(BlankPage, self).__init__(url, page_set=page_set)


class BlankPageSet(PageSet):
  def __init__(self):
    super(BlankPageSet, self).__init__(description='A single blank page.')
    self.AddPage(BlankPage('file://blank_page/blank_page.html', self))
