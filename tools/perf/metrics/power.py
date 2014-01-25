# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import logging

from metrics import Metric


class PowerMetric(Metric):
  """A metric for measuring power usage."""

  _enabed = True

  def __init__(self):
    super(PowerMetric, self).__init__()
    self._results = None

  @classmethod
  def CustomizeBrowserOptions(cls, options):
    PowerMetric._enabed = options.report_root_metrics

  def Start(self, _, tab):
    if not PowerMetric._enabed:
      return

    if not tab.browser.platform.CanMonitorPowerAsync():
      logging.warning("System doesn't support async power monitoring.")
      return

    tab.browser.platform.StartMonitoringPowerAsync()

  def Stop(self, _, tab):
    if not PowerMetric._enabed:
      return

    if not tab.browser.platform.CanMonitorPowerAsync():
      return

    self._results = tab.browser.platform.StopMonitoringPowerAsync()

  def AddResults(self, _, results):
    if not self._results:
      return

    energy_consumption_mwh = self._results['energy_consumption_mwh']
    results.Add('energy_consumption_mwh', 'mWh', energy_consumption_mwh)
    self._results = None
