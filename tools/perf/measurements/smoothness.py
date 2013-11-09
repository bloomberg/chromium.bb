# Copyright (c) 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from metrics import smoothness
from metrics import timeline
from telemetry.page import page_test
from telemetry.page import page_measurement


class MissingDisplayFrameRateError(page_measurement.MeasurementFailure):
  def __init__(self, name):
    super(MissingDisplayFrameRateError, self).__init__(
        'Missing display frame rate metrics: ' + name)


class Smoothness(page_measurement.PageMeasurement):
  def __init__(self):
    super(Smoothness, self).__init__('smoothness')
    self._metric = None

  def AddCommandLineOptions(self, parser):
    metric_choices = ['smoothness', 'timeline']
    parser.add_option('--metric', dest='metric', type='choice',
                      choices=metric_choices,
                      default='smoothness',
                      help=('Metric to use in the measurement. ' +
                            'Supported values: ' + ', '.join(metric_choices)))

  def CustomizeBrowserOptions(self, options):
    options.AppendExtraBrowserArgs('--enable-gpu-benchmarking')

  def CanRunForPage(self, page):
    return hasattr(page, 'smoothness')

  def WillRunAction(self, page, tab, action):
    if self.options.metric == 'smoothness':
      compound_action = page_test.GetCompoundActionFromPage(
          page, self._action_name_to_run)
      self._metric = smoothness.SmoothnessMetric(compound_action)
    elif self.options.metric == 'timeline':
      self._metric = timeline.TimelineMetric(timeline.TRACING_MODE)

    self._metric.Start(page, tab)

    if tab.browser.platform.IsRawDisplayFrameRateSupported():
      tab.browser.platform.StartRawDisplayFrameRateMeasurement()

  def DidRunAction(self, page, tab, action):
    if tab.browser.platform.IsRawDisplayFrameRateSupported():
      tab.browser.platform.StopRawDisplayFrameRateMeasurement()
    self._metric.Stop(page, tab)

  def MeasurePage(self, page, tab, results):
    self._metric.AddResults(tab, results)

    if tab.browser.platform.IsRawDisplayFrameRateSupported():
      for r in tab.browser.platform.GetRawDisplayFrameRateMeasurements():
        if r.value is None:
          raise MissingDisplayFrameRateError(r.name)
        results.Add(r.name, r.unit, r.value)
