# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import unittest

from metrics import smoothness

from telemetry.page import page


class FakePlatform(object):
  def IsRawDisplayFrameRateSupported(self):
    return False


class FakeBrowser(object):
  def __init__(self):
    self.platform = FakePlatform()
    self.category_filter = None

  def StartTracing(self, category_filter, _):
    self.category_filter = category_filter


class FakeTab(object):
  def __init__(self):
    self.browser = FakeBrowser()

  def ExecuteJavaScript(self, js):
    pass


class SmoothnessMetricUnitTest(unittest.TestCase):
  def testSyntheticDelayConfiguration(self):
    attributes = {
      'synthetic_delays': {
        'cc.BeginMainFrame': { 'target_duration': 0.012 },
        'cc.DrawAndSwap': { 'target_duration': 0.012, 'mode': 'alternating' },
        'gpu.SwapBuffers': { 'target_duration': 0.012 }
      }
    }
    test_page = page.Page('http://dummy', None, attributes=attributes)

    tab = FakeTab()
    smoothness_metric = smoothness.SmoothnessMetric()
    smoothness_metric.Start(test_page, tab)

    expected_category_filter = [
        'DELAY(cc.BeginMainFrame;0.012000;static)',
        'DELAY(cc.DrawAndSwap;0.012000;alternating)',
        'DELAY(gpu.SwapBuffers;0.012000;static)',
        'benchmark',
        'webkit.console'
    ]
    self.assertEquals(expected_category_filter,
                      sorted(tab.browser.category_filter.split(',')))
