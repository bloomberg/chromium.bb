# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from measurements import startup
from metrics import startup_metric

class StartWithUrl(startup.Startup):
  """Measures Chromium's startup performance when started with a URL

  This test inherits support for the --warm or --cold command line options -
  see startup.py for details.
  """

  def __init__(self):
    super(StartWithUrl, self).__init__()
    self.close_tabs_before_run = False


  def AddCommandLineOptions(self, parser):
    super(StartWithUrl, self).AddCommandLineOptions(parser)
    parser.add_option('--url', action='store', default=None,
                      help='Start with a request to open a specific URL')

  def CustomizeBrowserOptions(self, options):
    super(StartWithUrl, self).CustomizeBrowserOptions(options)
    if options.url:
      browser_options = options.browser_options
      browser_options.startup_url = options.url
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
      wpr_archives[page_set.WprFilePathForPage(page)] = True

    if len(wpr_archives.keys()) > 1:
      raise Exception("Invalid pageset: more than 1 WPR archive found.: " +
          ', '.join(wpr_archives.keys()))

  def MeasurePage(self, page, tab, results):
    # Wait for all tabs to finish loading.
    for i in xrange(len(tab.browser.tabs)):
      t = tab.browser.tabs[i]
      t.WaitForDocumentReadyStateToBeComplete()

    startup_metric.StartupMetric().AddResults(tab, results)

