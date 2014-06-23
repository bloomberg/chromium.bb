# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import collections

from measurements import startup
from metrics import cpu
from metrics import histogram_util
from metrics import startup_metric
from telemetry.core import util


class SessionRestore(startup.Startup):
  """Performs a measurement of Chromium's Session restore performance.

  This test is meant to be run against a generated profile.
  This test inherits support for the --warm or --cold command line options -
  see startup.py for details.
  """

  def __init__(self, action_name_to_run = ''):
    super(SessionRestore, self).__init__(action_name_to_run=action_name_to_run)
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

  def CanRunForPage(self, page):
    # No matter how many pages in the pageset, just perform one test iteration.
    return page.page_set.pages.index(page) == 0

  def RunNavigateSteps(self, page, tab):
    # Overriden so that no page navigation occurs.
    pass

  def ValidatePageSet(self, page_set):
    wpr_archive_names_to_page_urls = collections.defaultdict(list)
    # Construct the map from pages' wpr archive names to pages' urls.
    for page in page_set:
      if page.is_local:
        continue
      wpr_archive_name = page_set.WprFilePathForPage(page)
      wpr_archive_names_to_page_urls[wpr_archive_name].append(page.url)

    # Reject any pageset that contains more than one WPR archive.
    if len(wpr_archive_names_to_page_urls.keys()) > 1:
      raise Exception("Invalid pageset: more than 1 WPR archive found.: " +
          repr(wpr_archive_names_to_page_urls))

  def DidStartBrowser(self, browser):
    self._cpu_metric = cpu.CpuMetric(browser)
    self._cpu_metric.Start(None, None)

  def MeasurePage(self, page, tab, results):
    tab.WaitForDocumentReadyStateToBeComplete()

    # Record CPU usage from browser start to when the foreground page is loaded.
    self._cpu_metric.Stop(None, None)
    self._cpu_metric.AddResults(tab, results, 'cpu_utilization')

    startup_metric.StartupMetric().AddResults(tab, results)

    # TODO(jeremy): Measure time to load - first, last and frontmost tab here.
