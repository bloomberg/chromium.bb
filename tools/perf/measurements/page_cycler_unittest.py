# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import unittest

from telemetry.core import browser_options
from telemetry.page import page_runner
from telemetry.results import page_test_results
from telemetry.unittest import simple_mock

from measurements import page_cycler


# Allow testing protected members in the unit test.
# pylint: disable=W0212

class MockMemoryMetric(object):
  """Used instead of simple_mock.MockObject so that the precise order and
  number of calls need not be specified."""
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


class FakePage(object):
  """Used to mock loading a page."""
  def __init__(self, url):
    self.url = url


class FakeTab(object):
  """Used to mock a browser tab."""
  def __init__(self):
    self.clear_cache_calls = 0
  def ClearCache(self, force=False):
    assert force
    self.clear_cache_calls += 1
  def EvaluateJavaScript(self, _):
    return 1
  def WaitForJavaScriptExpression(self, _, __):
    pass
  @property
  def browser(self):
    return FakeBrowser()

class FakeBrowser(object):
  _iteration = 0

  @property
  def cpu_stats(self):
    FakeBrowser._iteration += 1
    return {
        'Browser': {'CpuProcessTime': FakeBrowser._iteration,
                    'TotalTime': FakeBrowser._iteration * 2},
        'Renderer': {'CpuProcessTime': FakeBrowser._iteration,
                    'TotalTime': FakeBrowser._iteration * 3},
        'Gpu': {'CpuProcessTime': FakeBrowser._iteration,
                 'TotalTime': FakeBrowser._iteration * 4}
    }
  @property
  def platform(self):
    return FakePlatform()


class FakePlatform(object):
  def GetOSName(self):
    return 'fake'
  def CanMonitorPower(self):
    return False


class PageCyclerUnitTest(unittest.TestCase):

  def SetUpCycler(self, args, setup_memory_module=False):
    cycler = page_cycler.PageCycler()
    options = browser_options.BrowserFinderOptions()
    options.browser_options.platform = FakePlatform()
    parser = options.CreateParser()
    page_runner.AddCommandLineArgs(parser)
    cycler.AddCommandLineArgs(parser)
    cycler.SetArgumentDefaults(parser)
    parser.parse_args(args)
    page_runner.ProcessCommandLineArgs(parser, options)
    cycler.ProcessCommandLineArgs(parser, options)
    cycler.CustomizeBrowserOptions(options.browser_options)

    if setup_memory_module:
      # Mock out memory metrics; the real ones require a real browser.
      mock_memory_metric = MockMemoryMetric()

      mock_memory_module = simple_mock.MockObject()
      mock_memory_module.ExpectCall(
        'MemoryMetric').WithArgs(simple_mock.DONT_CARE).WillReturn(
        mock_memory_metric)

      real_memory_module = page_cycler.memory
      try:
        page_cycler.memory = mock_memory_module
        browser = FakeBrowser()
        cycler.WillStartBrowser(browser)
        cycler.DidStartBrowser(browser)
      finally:
        page_cycler.memory = real_memory_module

    return cycler

  def testOptionsColdLoadNoArgs(self):
    cycler = self.SetUpCycler([])

    self.assertEquals(cycler._cold_run_start_index, 5)

  def testOptionsColdLoadPagesetRepeat(self):
    cycler = self.SetUpCycler(['--pageset-repeat=20', '--page-repeat=2'])

    self.assertEquals(cycler._cold_run_start_index, 20)

  def testOptionsColdLoadRequested(self):
    cycler = self.SetUpCycler(['--pageset-repeat=21', '--page-repeat=2',
                               '--cold-load-percent=40'])

    self.assertEquals(cycler._cold_run_start_index, 26)

  def testCacheHandled(self):
    cycler = self.SetUpCycler(['--pageset-repeat=5',
                               '--cold-load-percent=50'],
                              True)

    url_name = 'http://fakepage.com'
    page = FakePage(url_name)
    tab = FakeTab()

    for i in range(5):
      results = page_test_results.PageTestResults()
      results.WillRunPage(page)
      cycler.WillNavigateToPage(page, tab)
      self.assertEqual(max(0, i - 2), tab.clear_cache_calls,
                       'Iteration %d tab.clear_cache_calls %d' %
                       (i, tab.clear_cache_calls))
      cycler.MeasurePage(page, tab, results)
      results.DidRunPage(page)

      values = results.all_page_specific_values
      self.assertGreater(len(values), 2)

      self.assertEqual(values[0].page, page)
      chart_name = 'cold_times' if i == 0 or i > 2 else 'warm_times'
      self.assertEqual(values[0].name, '%s.page_load_time' % chart_name)
      self.assertEqual(values[0].units, 'ms')

      cycler.DidNavigateToPage(page, tab)

  def testColdWarm(self):
    cycler = self.SetUpCycler(['--pageset-repeat=3'], True)
    pages = [FakePage('http://fakepage1.com'), FakePage('http://fakepage2.com')]
    tab = FakeTab()
    for i in range(3):
      for page in pages:
        results = page_test_results.PageTestResults()
        results.WillRunPage(page)
        cycler.WillNavigateToPage(page, tab)
        cycler.MeasurePage(page, tab, results)
        results.DidRunPage(page)

        values = results.all_page_specific_values
        self.assertGreater(len(values), 2)

        self.assertEqual(values[0].page, page)

        chart_name = 'cold_times' if i == 0 or i > 1 else 'warm_times'
        self.assertEqual(values[0].name, '%s.page_load_time' % chart_name)
        self.assertEqual(values[0].units, 'ms')

        cycler.DidNavigateToPage(page, tab)

  def testResults(self):
    cycler = self.SetUpCycler([], True)

    pages = [FakePage('http://fakepage1.com'), FakePage('http://fakepage2.com')]
    tab = FakeTab()

    for i in range(2):
      for page in pages:
        results = page_test_results.PageTestResults()
        results.WillRunPage(page)
        cycler.WillNavigateToPage(page, tab)
        cycler.MeasurePage(page, tab, results)
        results.DidRunPage(page)

        values = results.all_page_specific_values
        self.assertEqual(4, len(values))

        self.assertEqual(values[0].page, page)
        chart_name = 'cold_times' if i == 0 else 'warm_times'
        self.assertEqual(values[0].name, '%s.page_load_time' % chart_name)
        self.assertEqual(values[0].units, 'ms')

        for value, expected in zip(values[1:], ['gpu', 'renderer', 'browser']):
          self.assertEqual(value.page, page)
          self.assertEqual(value.name,
                           'cpu_utilization.cpu_utilization_%s' % expected)
          self.assertEqual(value.units, '%')

        cycler.DidNavigateToPage(page, tab)
