# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from metrics import power
from telemetry.page import legacy_page_test


class Power(legacy_page_test.LegacyPageTest):
  """Measures power draw and idle wakeups during the page's interactions."""

  def __init__(self):
    super(Power, self).__init__()
    self._power_metric = None

  def WillStartBrowser(self, platform):
    self._power_metric = power.PowerMetric(platform)

  def WillNavigateToPage(self, page, tab):
    pass

  def DidNavigateToPage(self, page, tab):
    self._power_metric.Start(page, tab)

  def ValidateAndMeasurePage(self, page, tab, results):
    self._power_metric.Stop(page, tab)
    self._power_metric.AddResults(tab, results)

  def DidRunPage(self, platform):
    del platform  # unused
    self._power_metric.Close()
