# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from metrics import smoothness
from metrics import timeline_interaction_record as tir_module
from telemetry.core.timeline.model import TimelineModel
from telemetry.page import page_measurement

import telemetry.core.timeline.bounds as timeline_bounds


class MissingDisplayFrameRateError(page_measurement.MeasurementFailure):
  def __init__(self, name):
    super(MissingDisplayFrameRateError, self).__init__(
        'Missing display frame rate metrics: ' + name)

class SmoothnessController(object):
  def __init__(self):
    self._actions = []
    self._timeline_model = None

  def Start(self, page, tab):
    self._actions = []
    custom_categories = ['webkit.console', 'benchmark']
    custom_categories += page.GetSyntheticDelayCategories()
    tab.browser.StartTracing(','.join(custom_categories), 60)
    if tab.browser.platform.IsRawDisplayFrameRateSupported():
      tab.browser.platform.StartRawDisplayFrameRateMeasurement()

  def AddActionToIncludeInMetric(self, action):
    self._actions.append(action)

  def Stop(self, tab):
    # Stop tracing for smoothness metric.
    if tab.browser.platform.IsRawDisplayFrameRateSupported():
      tab.browser.platform.StopRawDisplayFrameRateMeasurement()
    tracing_timeline_data = tab.browser.StopTracing()
    self._timeline_model = TimelineModel(timeline_data=tracing_timeline_data)

  def AddResults(self, tab, results):
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
    smoothness_metric = smoothness.SmoothnessMetric()
    smoothness_metric.AddResults(self._timeline_model,
                                       renderer_thread,
                                       interaction_record,
                                       results)
    if tab.browser.platform.IsRawDisplayFrameRateSupported():
      for r in tab.browser.platform.GetRawDisplayFrameRateMeasurements():
        if r.value is None:
          raise MissingDisplayFrameRateError(r.name)
        results.Add(r.name, r.unit, r.value)
