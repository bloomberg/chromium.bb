# Copyright (c) 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os

from telemetry.core import util
from telemetry.page import page_set
from telemetry.page import profile_creator

class SmallProfileCreator(profile_creator.ProfileCreator):
  """
  Runs a browser through a series of operations to fill in a small test profile.
  """

  def __init__(self):
    super(SmallProfileCreator, self).__init__()
    top_25 = os.path.join(util.GetBaseDir(), 'page_sets', 'top_25.json')
    self._page_set = page_set.PageSet.FromFile(top_25)

  def MeasurePage(self, _, tab, results):
    # Multiple tabs would help make this faster, but that can't be done until
    # crbug.com/258113 is fixed.
    tab.WaitForDocumentReadyStateToBeComplete()
