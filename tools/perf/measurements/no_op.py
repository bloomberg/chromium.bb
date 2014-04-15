# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from telemetry.page import page_measurement

class NoOp(page_measurement.PageMeasurement):
  def __init__(self):
    super(NoOp, self).__init__('RunNoOp')

  def MeasurePage(self, page, tab, results):
    pass
