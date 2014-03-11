# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import logging

from metrics import Metric


class PowerMetric(Metric):
  """A metric for measuring power usage."""

  _enabled = True

  def __init__(self):
    super(PowerMetric, self).__init__()
    self._browser = None
    self._running = False
    self._results = None

  def __del__(self):
    # TODO(jeremy): Remove once crbug.com/350841 is fixed.
    # Don't leave power monitoring processes running on the system.
    self._StopInternal()
    parent = super(PowerMetric, self)
    if hasattr(parent, '__del__'):
      parent.__del__()

  def _StopInternal(self):
    """ Stop monitoring power if measurement is running. This function is
    idempotent."""
    if not self._running:
      return
    self._running = False
    self._results = self._browser.platform.StopMonitoringPowerAsync()

  @classmethod
  def CustomizeBrowserOptions(cls, options):
    PowerMetric._enabled = options.report_root_metrics

  def Start(self, _, tab):
    if not PowerMetric._enabled:
      return

    if not tab.browser.platform.CanMonitorPowerAsync():
      logging.warning("System doesn't support async power monitoring.")
      return

    self._results = None
    self._browser = tab.browser
    self._StopInternal()

    self._browser.platform.StartMonitoringPowerAsync()
    self._running = True

  def Stop(self, _, tab):
    if not PowerMetric._enabled:
      return

    if not tab.browser.platform.CanMonitorPowerAsync():
      return

    self._StopInternal()

  def AddResults(self, _, results):
    if not self._results:
      return

    energy_consumption_mwh = self._results['energy_consumption_mwh']
    results.Add('energy_consumption_mwh', 'mWh', energy_consumption_mwh)
    self._results = None
