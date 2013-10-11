# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from metrics import cpu
from metrics import media
from metrics import memory
from telemetry.page import page_measurement


class Media(page_measurement.PageMeasurement):
  """The MediaMeasurement class gathers media-related metrics on a page set.

  Media metrics recorded are controlled by metrics/media.js.  At the end of the
  test each metric for every media element in the page are reported.
  """

  def __init__(self):
    super(Media, self).__init__('media_metrics')
    self._media_metric = None
    # Used to add browser memory and CPU metrics to results per test.
    self._add_browser_metrics = False
    self._cpu_metric = None
    self._memory_metric = None

  def results_are_the_same_on_every_page(self):
    """Results can vary from page to page based on media events taking place."""
    return False

  def DidNavigateToPage(self, page, tab):
    """Override to do operations right after the page is navigated."""
    self._media_metric = media.MediaMetric(tab)
    self._media_metric.Start(page, tab)
    # Reset to false for every page.
    self._add_browser_metrics = False
    if hasattr(page, 'add_browser_metrics'):
      self._add_browser_metrics = page.add_browser_metrics

    # Start browser metrics at page level to isolate startup CPU usage.
    if self._add_browser_metrics:
      self._cpu_metric = cpu.CpuMetric(tab.browser)
      self._cpu_metric.Start(page, tab)
      # No need to start memory metric since we are only interested in Summary
      # results.
      self._memory_metric = memory.MemoryMetric(tab.browser)

  def MeasurePage(self, page, tab, results):
    """Measure the page's performance."""
    self._media_metric.Stop(page, tab)
    trace_name = self._media_metric.AddResults(tab, results)
    if self._add_browser_metrics:
      self._cpu_metric.Stop(page, tab)
      self._cpu_metric.AddResults(tab, results, trace_name=trace_name)
      self._memory_metric.AddSummaryResults(results, trace_name=trace_name)
