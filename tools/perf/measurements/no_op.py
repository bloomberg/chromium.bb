# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from telemetry.page import page_test


class NoOp(page_test.PageTest):

  def __init__(self):
    super(NoOp, self).__init__()

  def ValidateAndMeasurePage(self, page, tab, results):
    pass
