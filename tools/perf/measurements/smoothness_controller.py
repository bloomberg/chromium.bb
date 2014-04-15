# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from metrics import smoothness
from telemetry.core.timeline.model import TimelineModel
from telemetry.page import page_measurement
from telemetry.page.actions import action_runner
from telemetry.web_perf import timeline_interaction_record as tir_module


class MissingDisplayFrameRateError(page_measurement.MeasurementFailure):
  def __init__(self, name):
    super(MissingDisplayFrameRateError, self).__init__(
        'Missing display frame rate metrics: ' + name)

class SmoothnessController(object):
  def __init__(self):
    self._timeline_model = None

  def Start(self, page, tab):
    custom_categories = ['webkit.console', 'benchmark']
    custom_categories += page.GetSyntheticDelayCategories()
    tab.browser.StartTracing(','.join(custom_categories), 60)
    if tab.browser.platform.IsRawDisplayFrameRateSupported():
      tab.browser.platform.StartRawDisplayFrameRateMeasurement()
    # Start the smooth marker for all smooth actions.
    runner = action_runner.ActionRunner(None, tab)
    runner.BeginInteraction('RunSmoothActions', [tir_module.IS_SMOOTH])

  def Stop(self, tab):
    # End the smooth marker for all smooth actions.
    runner = action_runner.ActionRunner(None, tab)
    runner.EndInteraction('RunSmoothActions', [tir_module.IS_SMOOTH])
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
    # Create an interaction_record for this legacy measurement. Since we don't
    # wrap the results that is sent to smoothnes metric, the logical_name will
    # not be used.
    renderer_thread = self._timeline_model.GetRendererThreadFromTab(tab)
    records = [tir_module.TimelineInteractionRecord.FromEvent(event) for
               event in renderer_thread.async_slices
               if tir_module.IsTimelineInteractionRecord(event.name)]
    assert len(records) == 1
    smoothness_metric = smoothness.SmoothnessMetric()
    smoothness_metric.AddResults(self._timeline_model,
                                       renderer_thread,
                                       records[0],
                                       results)
    if tab.browser.platform.IsRawDisplayFrameRateSupported():
      for r in tab.browser.platform.GetRawDisplayFrameRateMeasurements():
        if r.value is None:
          raise MissingDisplayFrameRateError(r.name)
        results.Add(r.name, r.unit, r.value)

  def CleanUp(self, tab):
    if tab.browser.platform.IsRawDisplayFrameRateSupported():
      tab.browser.platform.StopRawDisplayFrameRateMeasurement()
    if tab.browser.is_tracing_running:
      tab.browser.StopTracing()
