# Copyright 2015 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import logging

import chrome_proxy_metrics as metrics
from telemetry.core import exceptions
from telemetry.page import page_test

class ChromeProxyLatencyBase(page_test.PageTest):
  """Chrome latency measurement."""

  def __init__(self, *args, **kwargs):
    super(ChromeProxyLatencyBase, self).__init__(*args, **kwargs)
    self._metrics = metrics.ChromeProxyMetric()

  def WillNavigateToPage(self, page, tab):
    tab.ClearCache(force=True)

  def ValidateAndMeasurePage(self, page, tab, results):
    # Wait for the load event.
    tab.WaitForJavaScriptExpression('performance.timing.loadEventStart', 300)
    self._metrics.AddResultsForLatency(tab, results)


class ChromeProxyLatency(ChromeProxyLatencyBase):
  """Chrome proxy latency measurement."""

  def __init__(self, *args, **kwargs):
    super(ChromeProxyLatency, self).__init__(*args, **kwargs)

  def CustomizeBrowserOptions(self, options):
    options.AppendExtraBrowserArgs('--enable-spdy-proxy-auth')


class ChromeProxyLatencyDirect(ChromeProxyLatencyBase):
  """Direct connection latency measurement."""

  def __init__(self, *args, **kwargs):
    super(ChromeProxyLatencyDirect, self).__init__(*args, **kwargs)


class ChromeProxyDataSavingBase(page_test.PageTest):
  """Chrome data saving measurement."""
  def __init__(self, *args, **kwargs):
    super(ChromeProxyDataSavingBase, self).__init__(*args, **kwargs)
    self._metrics = metrics.ChromeProxyMetric()

  def WillNavigateToPage(self, page, tab):
    tab.ClearCache(force=True)
    self._metrics.Start(page, tab)

  def ValidateAndMeasurePage(self, page, tab, results):
    # Wait for the load event.
    tab.WaitForJavaScriptExpression('performance.timing.loadEventStart', 300)
    self._metrics.Stop(page, tab)
    self._metrics.AddResultsForDataSaving(tab, results)


class ChromeProxyDataSaving(ChromeProxyDataSavingBase):
  """Chrome proxy data saving measurement."""

  def __init__(self, *args, **kwargs):
    super(ChromeProxyDataSaving, self).__init__(*args, **kwargs)

  def CustomizeBrowserOptions(self, options):
    options.AppendExtraBrowserArgs('--enable-spdy-proxy-auth')


class ChromeProxyDataSavingDirect(ChromeProxyDataSavingBase):
  """Direct connection data saving measurement."""

  def __init__(self, *args, **kwargs):
    super(ChromeProxyDataSavingDirect, self).__init__(*args, **kwargs)
