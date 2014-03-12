# Copyright (c) 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from metrics import power
from metrics import smoothness
from metrics import timeline_interaction_record as tir_module
from telemetry.core.timeline.model import TimelineModel
from telemetry.page import page_measurement

import telemetry.core.timeline.bounds as timeline_bounds


class MissingDisplayFrameRateError(page_measurement.MeasurementFailure):
  def __init__(self, name):
    super(MissingDisplayFrameRateError, self).__init__(
        'Missing display frame rate metrics: ' + name)


class Smoothness(page_measurement.PageMeasurement):
  def __init__(self):
    super(Smoothness, self).__init__('smoothness')
    self._smoothness_metric = None
    self._power_metric = None
    self._timeline_model = None
    self._actions = []

  def CustomizeBrowserOptions(self, options):
    options.AppendExtraBrowserArgs('--enable-gpu-benchmarking')
    power.PowerMetric.CustomizeBrowserOptions(options)

  def CanRunForPage(self, page):
    return hasattr(page, 'smoothness')

  def WillRunActions(self, page, tab):
    self._power_metric = power.PowerMetric()
    self._power_metric.Start(page, tab)
    self._smoothness_metric = smoothness.SmoothnessMetric()
    # Start tracing for smoothness metric.
    custom_categories = ['webkit.console', 'benchmark']
    custom_categories += page.GetSyntheticDelayCategories()
    tab.browser.StartTracing(','.join(custom_categories), 60)
    if tab.browser.platform.IsRawDisplayFrameRateSupported():
      tab.browser.platform.StartRawDisplayFrameRateMeasurement()

  def DidRunAction(self, page, tab, action):
    self._actions.append(action)

  def DidRunActions(self, page, tab):
    self._power_metric.Stop(page, tab)
    # Stop tracing for smoothness metric.
    if tab.browser.platform.IsRawDisplayFrameRateSupported():
      tab.browser.platform.StopRawDisplayFrameRateMeasurement()
    tracing_timeline_data = tab.browser.StopTracing()
    self._timeline_model = TimelineModel(timeline_data=tracing_timeline_data)

  def MeasurePage(self, page, tab, results):
    self._power_metric.AddResults(tab, results)
    # Add results of smoothness metric. This computes the smoothness metric for
    # the time range between the first action starts and the last action ends.
    # To get the measurement for each action, use
    # measurement.TimelineBasedMeasurement.
    time_bounds = timeline_bounds.Bounds()
    for action in self._actions:
      time_bounds.AddBounds(
        action.GetActiveRangeOnTimeline(self._timeline_model))
    # Create an interaction_record for this legacy measurement. Since we don't
    # wrap the results that is sent to smoothnes metric, the logical_name will
    # not be used.
    interaction_record = tir_module.TimelineInteractionRecord(
      'smoothness_interaction', time_bounds.min, time_bounds.max)
    renderer_thread = self._timeline_model.GetRendererThreadFromTab(tab)
    self._smoothness_metric.AddResults(self._timeline_model,
                                       renderer_thread,
                                       interaction_record,
                                       results)
    if tab.browser.platform.IsRawDisplayFrameRateSupported():
      for r in tab.browser.platform.GetRawDisplayFrameRateMeasurements():
        if r.value is None:
          raise MissingDisplayFrameRateError(r.name)
        results.Add(r.name, r.unit, r.value)
