# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
import unittest

from measurements import page_cycler
from telemetry.core import browser_options
from telemetry.page import page_measurement_results
from telemetry.unittest import simple_mock

# Allow testing protected members in the unit test.
# pylint: disable=W0212

# Used instead of simple_mock.MockObject so that the precise order and
# number of calls need not be specified.
class MockMemoryMetric(object):
  def __init__(self):
    pass

  def Start(self, page, tab):
    pass

  def Stop(self, page, tab):
    pass

  def AddResults(self, tab, results):
    pass

  def AddSummaryResults(self, tab, results):
    pass

class PageCyclerUnitTest(unittest.TestCase):

  # TODO(tonyg): Remove this backfill when we can assume python 2.7 everywhere.
  def assertIn(self, first, second, _=None):
    self.assertTrue(first in second,
                    msg="'%s' not found in '%s'" % (first, second))

  def setupCycler(self, args):
    cycler = page_cycler.PageCycler()
    options = browser_options.BrowserFinderOptions()
    parser = options.CreateParser()
    cycler.AddCommandLineOptions(parser)
    parser.parse_args(args)
    cycler.CustomizeBrowserOptions(options)

    return cycler

  def testOptionsColdLoadNoArgs(self):
    cycler = self.setupCycler([])

    self.assertFalse(cycler._cold_runs_requested)
    self.assertEqual(cycler._number_warm_runs, 9)

  def testOptionsColdLoadPagesetRepeat(self):
    cycler = self.setupCycler(['--pageset-repeat=20', '--page-repeat=2'])

    self.assertFalse(cycler._cold_runs_requested)
    self.assertEqual(cycler._number_warm_runs, 38)

  def testOptionsColdLoadRequested(self):
    cycler = self.setupCycler(['--pageset-repeat=21', '--page-repeat=2',
                               '--cold-load-percent=40'])

    self.assertTrue(cycler._cold_runs_requested)
    self.assertEqual(cycler._number_warm_runs, 24)

  def testIncompatibleOptions(self):
    exception_seen = False
    try:
      self.setupCycler(['--pageset-repeat=20s', '--page-repeat=2s',
                        '--cold-load-percent=40'])
    except Exception as e:
      exception_seen = True
      self.assertEqual('--cold-load-percent is incompatible with '
                       'timed repeat', e.args[0])

    self.assertTrue(exception_seen)

  def testCacheHandled(self):
    cycler = self.setupCycler(['--pageset-repeat=5',
                               '--cold-load-percent=50'])

    # Mock out memory metrics; the real ones require a real browser.
    mock_memory_metric = MockMemoryMetric()

    mock_memory_module = simple_mock.MockObject()
    mock_memory_module.ExpectCall(
      'MemoryMetric').WithArgs(simple_mock.DONT_CARE).WillReturn(
      mock_memory_metric)

    real_memory_module = page_cycler.memory
    try:
      page_cycler.memory = mock_memory_module
      cycler.DidStartBrowser(None)
    finally:
      page_cycler.memory = real_memory_module

    class FakePage(object):
      def __init__(self, url):
        self.url = url

    class FakeTab(object):
      def __init__(self):
        self.clear_cache_calls = 0
      def ClearCache(self):
        self.clear_cache_calls += 1
      def EvaluateJavaScript(self, _):
        return 1
      def WaitForJavaScriptExpression(self, _, __):
        pass

    url_name = "http://fakepage.com"
    page = FakePage(url_name)
    tab = FakeTab()
    results = page_measurement_results.PageMeasurementResults()

    for i in range(5):
      cycler.WillNavigateToPage(page, tab)
      self.assertEqual(max(0, i - 2), tab.clear_cache_calls,
                       "Iteration %d tab.clear_cache_calls %d" %
                       (i, tab.clear_cache_calls))
      num_results = len(results.page_results)
      results.WillMeasurePage(page)
      cycler.MeasurePage(page, tab, results)
      results.DidMeasurePage()
      self.assertEqual(num_results+1, len(results.page_results))
      result = results[-1]
      self.assertEqual(result.page, page)
      self.assertEqual(1, len(result.values))
      self.assertEqual(result.values[0].trace_name, 'page_load_time')
      self.assertEqual(result.values[0].units, 'ms')
      self.assertEqual(result.values[0].chart_name,
                       'warm_times' if i < 3 else 'cold_times')
      cycler.DidNavigateToPage(page, tab)
