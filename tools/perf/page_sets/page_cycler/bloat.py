# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
# pylint: disable=W0401,W0614
from telemetry.page.actions.all_page_actions import *
from telemetry.page import page as page_module
from telemetry.page import page_set as page_set_module


class BloatPage(page_module.PageWithDefaultRunNavigate):

  def __init__(self, url, page_set):
    super(BloatPage, self).__init__(url=url, page_set=page_set)


class BloatPageSet(page_set_module.PageSet):

  """ Bloat page_cycler benchmark """

  def __init__(self):
    super(BloatPageSet, self).__init__(
      # pylint: disable=C0301
      serving_dirs=set(['../../../../data/page_cycler/bloat']))

    self.AddPage(BloatPage(
      'file://../../../../data/page_cycler/bloat/gmail_load_cleardot/',
      self))
