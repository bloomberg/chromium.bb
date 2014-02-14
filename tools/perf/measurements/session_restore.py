# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from measurements import startup
from metrics import cpu
from metrics import startup_metric


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
    options.AppendExtraBrowserArgs([
        '--restore-last-session'
    ])

  def CanRunForPage(self, page):
    # No matter how many pages in the pageset, just perform one test iteration.
    return page.page_set.pages.index(page) == 0

  def RunNavigateSteps(self, page, tab):
    # Overriden so that no page navigation occurs.
    pass

  def ValidatePageSet(self, page_set):
    # Reject any pageset that contains more than one WPR archive.
    wpr_archives = {}
    for page in page_set:
      if not page.is_local:
        wpr_archives[page_set.WprFilePathForPage(page)] = True

    if len(wpr_archives.keys()) > 1:
      raise Exception("Invalid pageset: more than 1 WPR archive found.: " +
          ', '.join(wpr_archives.keys()))

  def DidStartBrowser(self, browser):
    self._cpu_metric = cpu.CpuMetric(browser)
    self._cpu_metric.Start(None, None)

  def MeasurePage(self, page, tab, results):

    tab.browser.foreground_tab.WaitForDocumentReadyStateToBeComplete()
    # Record CPU usage from browser start to when all pages have loaded.
    self._cpu_metric.Stop(None, None)
    self._cpu_metric.AddResults(tab, results, 'cpu_utilization')

    startup_metric.StartupMetric().AddResults(tab, results)

    # TODO(jeremy): Measure time to load - first, last and frontmost tab here.