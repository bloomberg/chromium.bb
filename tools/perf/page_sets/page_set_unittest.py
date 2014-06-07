# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os

from telemetry.unittest import page_set_smoke_test


class PageSetUnitTest(page_set_smoke_test.PageSetSmokeTest):

  def testSmoke(self):
    page_sets_dir = os.path.dirname(os.path.realpath(__file__))
    top_level_dir = os.path.dirname(page_sets_dir)
    self.RunSmokeTest(page_sets_dir, top_level_dir)
