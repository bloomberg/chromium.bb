# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import json
import logging

from metrics import histogram_util
from metrics import Metric
from telemetry.core import util


class StartupMetric(Metric):
  """A metric for browser startup time.

  User visible metrics:
    process_creation_to_window_display: Time from browser process creation to
        the time that the browser window initially becomes visible.
    process_creation_to_foreground_tab_loaded: Time from browser process
        creation to the time that the foreground tab is fully loaded.
    process_creation_to_all_tabs_loaded: Time from the browser process creation
        to the time that all tabs have fully loaded.

  Critical code progression:
    process_creation_to_main: Time from process creation to the execution of the
        browser's main() entry.
    main_to_messageloop_start: Time from main() entry to the start of the UI
        thread's message loop.
  """

  def Start(self, page, tab):
    raise NotImplementedError()

  def Stop(self, page, tab):
    raise NotImplementedError()

  def _GetBrowserMainEntryTime(self, tab):
    """Returns the main entry time (in ms) of the browser."""
    high_bytes = histogram_util.GetHistogramSum(
        histogram_util.BROWSER_HISTOGRAM,
        'Startup.BrowserMainEntryTimeAbsoluteHighWord',
        tab)
    low_bytes = histogram_util.GetHistogramSum(
        histogram_util.BROWSER_HISTOGRAM,
        'Startup.BrowserMainEntryTimeAbsoluteLowWord',
        tab)
    if high_bytes == 0 and low_bytes == 0:
      return None
    return (int(high_bytes) << 32) | (int(low_bytes) << 1)

  def _GetTabLoadTimes(self, browser):
    """Returns a tuple of (foreground_tab_load_time, all_tabs_load_time)."""
    foreground_tab_load_time = 0
    all_tabs_load_time = 0
    for i in xrange(len(browser.tabs)):
      try:
        tab = browser.tabs[i]
        tab.WaitForDocumentReadyStateToBeComplete()
        result = json.loads(tab.EvaluateJavaScript(
            'statsCollectionController.tabLoadTiming()'))
        load_time = result['load_start_ms'] + result['load_duration_ms']
        all_tabs_load_time = max(all_tabs_load_time, load_time)
        if tab == browser.foreground_tab:
          foreground_tab_load_time = load_time
      except util.TimeoutException:
        # Low memory Android devices may not be able to load more than
        # one tab at a time, so may timeout when the test attempts to
        # access a background tab. Ignore these tabs.
        logging.error('Tab timed out on JavaScript access')
    return foreground_tab_load_time, all_tabs_load_time

  def AddResults(self, tab, results):
    absolute_browser_main_entry_ms = self._GetBrowserMainEntryTime(tab)

    process_creation_to_window_display_ms = histogram_util.GetHistogramSum(
        histogram_util.BROWSER_HISTOGRAM, 'Startup.BrowserWindowDisplay', tab)
    absolute_foreground_tab_loaded_ms, absolute_all_tabs_loaded_ms = \
        self._GetTabLoadTimes(tab.browser)
    process_creation_to_messageloop_start_ms = histogram_util.GetHistogramSum(
        histogram_util.BROWSER_HISTOGRAM, 'Startup.BrowserMessageLoopStartTime',
        tab)
    main_to_messageloop_start_ms = histogram_util.GetHistogramSum(
        histogram_util.BROWSER_HISTOGRAM,
        'Startup.BrowserMessageLoopStartTimeFromMainEntry',
        tab)
    process_creation_to_main = (process_creation_to_messageloop_start_ms -
                                main_to_messageloop_start_ms)

    # User visible.
    results.Add('process_creation_to_window_display', 'ms',
                process_creation_to_window_display_ms)
    results.Add('process_creation_to_foreground_tab_loaded', 'ms',
                absolute_foreground_tab_loaded_ms -
                absolute_browser_main_entry_ms + process_creation_to_main)
    results.Add('process_creation_to_all_tabs_loaded', 'ms',
                absolute_all_tabs_loaded_ms -
                absolute_browser_main_entry_ms + process_creation_to_main)

    # Critical code progression.
    results.Add('process_creation_to_main', 'ms',
                process_creation_to_main)
    results.Add('main_to_messageloop_start', 'ms',
                main_to_messageloop_start_ms)
