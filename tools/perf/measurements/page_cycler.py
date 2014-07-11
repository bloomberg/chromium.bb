# Copyright 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""The page cycler measurement.

This measurement registers a window load handler in which is forces a layout and
then records the value of performance.now(). This call to now() measures the
time from navigationStart (immediately after the previous page's beforeunload
event) until after the layout in the page's load event. In addition, two garbage
collections are performed in between the page loads (in the beforeunload event).
This extra garbage collection time is not included in the measurement times.

Finally, various memory and IO statistics are gathered at the very end of
cycling all pages.
"""

import collections
import os

from metrics import cpu
from metrics import iometric
from metrics import memory
from metrics import power
from metrics import speedindex
from metrics import v8_object_stats
from telemetry.core import util
from telemetry.page import page_measurement
from telemetry.value import scalar


class PageCycler(page_measurement.PageMeasurement):
  options = {'pageset_repeat': 10}

  def __init__(self, *args, **kwargs):
    super(PageCycler, self).__init__(*args, **kwargs)

    with open(os.path.join(os.path.dirname(__file__),
                           'page_cycler.js'), 'r') as f:
      self._page_cycler_js = f.read()

    self._speedindex_metric = speedindex.SpeedIndexMetric()
    self._memory_metric = None
    self._power_metric = None
    self._cpu_metric = None
    self._v8_object_stats_metric = None
    self._has_loaded_page = collections.defaultdict(int)

  @classmethod
  def AddCommandLineArgs(cls, parser):
    parser.add_option('--v8-object-stats',
        action='store_true',
        help='Enable detailed V8 object statistics.')

    parser.add_option('--report-speed-index',
        action='store_true',
        help='Enable the speed index metric.')

    parser.add_option('--cold-load-percent', type='int', default=50,
                      help='%d of page visits for which a cold load is forced')

  @classmethod
  def ProcessCommandLineArgs(cls, parser, args):
    cls._record_v8_object_stats = args.v8_object_stats
    cls._report_speed_index = args.report_speed_index

    cold_runs_percent_set = (args.cold_load_percent != None)
    # Handle requests for cold cache runs
    if (cold_runs_percent_set and
        (args.cold_load_percent < 0 or args.cold_load_percent > 100)):
      raise Exception('--cold-load-percent must be in the range [0-100]')

    # Make sure _cold_run_start_index is an integer multiple of page_repeat.
    # Without this, --pageset_shuffle + --page_repeat could lead to
    # assertion failures on _started_warm in WillNavigateToPage.
    if cold_runs_percent_set:
      number_warm_pageset_runs = int(
          (int(args.pageset_repeat) - 1) * (100 - args.cold_load_percent) / 100)
      number_warm_runs = number_warm_pageset_runs * args.page_repeat
      cls._cold_run_start_index = number_warm_runs + args.page_repeat
      cls.discard_first_result = (not args.cold_load_percent or
                                  cls.discard_first_result)
    else:
      cls._cold_run_start_index = args.pageset_repeat * args.page_repeat

  def WillStartBrowser(self, browser):
    """Initialize metrics once right before the browser has been launched."""
    self._power_metric = power.PowerMetric(browser)

  def DidStartBrowser(self, browser):
    """Initialize metrics once right after the browser has been launched."""
    self._memory_metric = memory.MemoryMetric(browser)
    self._cpu_metric = cpu.CpuMetric(browser)
    if self._record_v8_object_stats:
      self._v8_object_stats_metric = v8_object_stats.V8ObjectStatsMetric()

  def DidStartHTTPServer(self, tab):
    # Avoid paying for a cross-renderer navigation on the first page on legacy
    # page cyclers which use the filesystem.
    tab.Navigate(tab.browser.http_server.UrlOf('nonexistent.html'))

  def WillNavigateToPage(self, page, tab):
    page.script_to_evaluate_on_commit = self._page_cycler_js
    if self.ShouldRunCold(page.url):
      tab.ClearCache(force=True)
    if self._report_speed_index:
      self._speedindex_metric.Start(page, tab)
    self._cpu_metric.Start(page, tab)
    self._power_metric.Start(page, tab)

  def DidNavigateToPage(self, page, tab):
    self._memory_metric.Start(page, tab)
    if self._record_v8_object_stats:
      self._v8_object_stats_metric.Start(page, tab)

  def CustomizeBrowserOptions(self, options):
    memory.MemoryMetric.CustomizeBrowserOptions(options)
    power.PowerMetric.CustomizeBrowserOptions(options)
    iometric.IOMetric.CustomizeBrowserOptions(options)
    options.AppendExtraBrowserArgs('--js-flags=--expose_gc')

    if self._record_v8_object_stats:
      v8_object_stats.V8ObjectStatsMetric.CustomizeBrowserOptions(options)
    if self._report_speed_index:
      self._speedindex_metric.CustomizeBrowserOptions(options)

  def MeasurePage(self, page, tab, results):
    tab.WaitForJavaScriptExpression('__pc_load_time', 60)

    chart_name_prefix = ('cold_' if self.IsRunCold(page.url) else
                         'warm_')

    results.AddValue(scalar.ScalarValue(
        results.current_page, '%stimes.page_load_time' % chart_name_prefix,
        'ms', tab.EvaluateJavaScript('__pc_load_time')))

    self._has_loaded_page[page.url] += 1

    self._power_metric.Stop(page, tab)
    self._memory_metric.Stop(page, tab)
    self._memory_metric.AddResults(tab, results)
    self._power_metric.AddResults(tab, results)

    self._cpu_metric.Stop(page, tab)
    self._cpu_metric.AddResults(tab, results)
    if self._record_v8_object_stats:
      self._v8_object_stats_metric.Stop(page, tab)
      self._v8_object_stats_metric.AddResults(tab, results)

    if self._report_speed_index:
      def SpeedIndexIsFinished():
        return self._speedindex_metric.IsFinished(tab)
      util.WaitFor(SpeedIndexIsFinished, 60)
      self._speedindex_metric.Stop(page, tab)
      self._speedindex_metric.AddResults(
          tab, results, chart_name=chart_name_prefix+'speed_index')

  def DidRunTest(self, browser, results):
    iometric.IOMetric().AddSummaryResults(browser, results)

  def IsRunCold(self, url):
    return (self.ShouldRunCold(url) or
            self._has_loaded_page[url] == 0)

  def ShouldRunCold(self, url):
    # We do the warm runs first for two reasons.  The first is so we can
    # preserve any initial profile cache for as long as possible.
    # The second is that, if we did cold runs first, we'd have a transition
    # page set during which we wanted the run for each URL to both
    # contribute to the cold data and warm the catch for the following
    # warm run, and clearing the cache before the load of the following
    # URL would eliminate the intended warmup for the previous URL.
    return (self._has_loaded_page[url] >= self._cold_run_start_index)

  def results_are_the_same_on_every_page(self):
    return False
