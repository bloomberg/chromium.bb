# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
# pylint: disable=W0401,W0614
from telemetry.page.actions.all_page_actions import *
from telemetry.page import page as page_module
from telemetry.page import page_set as page_set_module


class ToughLayerCasesPage(page_module.PageWithDefaultRunNavigate):

  def __init__(self, url, page_set):
    super(ToughLayerCasesPage, self).__init__(url=url, page_set=page_set)


class ToughLayerCasesPageSet(page_set_module.PageSet):

  """ A collection of tests to measure layer performance. """

  def __init__(self):
    super(ToughLayerCasesPageSet, self).__init__()

    self.AddPage(ToughLayerCasesPage(
      'file://layer_stress_tests/opacity.html', self))

