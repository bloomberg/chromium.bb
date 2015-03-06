# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import collections

from measurements import startup
from metrics import cpu
from metrics import startup_metric
from telemetry.core import util
from telemetry.value import histogram_util


class SessionRestore(startup.Startup):
  """Performs a measurement of Chromium's Session restore performance.

  This test is meant to be run against a generated profile.
  This test inherits support for the 'cold' option -
  see startup.py for details.
  """

  def __init__(self, cold=False, action_name_to_run='RunPageInteractions'):
    super(SessionRestore, self).__init__(cold=cold,
                                         action_name_to_run=action_name_to_run)
    self.close_tabs_before_run = False
    self._cpu_metric = None

  def CustomizeBrowserOptions(self, options):
    super(SessionRestore, self).CustomizeBrowserOptions(options)
    histogram_util.CustomizeBrowserOptions(options)
    options.AppendExtraBrowserArgs([
        '--restore-last-session'
    ])

  def TabForPage(self, page, browser):
    # Detect that the session restore has completed.
    util.WaitFor(lambda: browser.tabs and
                 histogram_util.GetHistogramCount(
                     histogram_util.BROWSER_HISTOGRAM,
                     'SessionRestore.AllTabsLoaded',
                     browser.foreground_tab),
                 60)
    return browser.foreground_tab

  def RunNavigateSteps(self, page, tab):
    # Overridden so that no page navigation occurs.
    pass

  def DidStartBrowser(self, browser):
    self._cpu_metric = cpu.CpuMetric(browser)
    self._cpu_metric.Start(None, None)

  def ValidateAndMeasurePage(self, page, tab, results):
    tab.WaitForDocumentReadyStateToBeComplete()
    super(SessionRestore, self).ValidateAndMeasurePage(page, tab, results)

    # Record CPU usage from browser start to when the foreground page is loaded.
    self._cpu_metric.Stop(None, None)
    self._cpu_metric.AddResults(tab, results, 'cpu_utilization')

    # TODO(jeremy): Measure time to load - first, last and frontmost tab here.
