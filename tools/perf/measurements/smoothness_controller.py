# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
import sys

from measurements import smooth_gesture_util
from telemetry.timeline.model import TimelineModel
from telemetry.page import page_measurement
from telemetry.page.actions import action_runner
from telemetry.value import list_of_scalar_values
from telemetry.value import scalar
from telemetry.web_perf import timeline_interaction_record as tir_module
from telemetry.web_perf.metrics import smoothness


RUN_SMOOTH_ACTIONS = 'RunSmoothAllActions'

# Descriptions for results from platform.GetRawDisplayFrameRateMeasurements().
DESCRIPTIONS = {
    'avg_surface_fps': 'Average frames per second as measured by the '
                       'platform\'s SurfaceFlinger.'
}


class MissingDisplayFrameRateError(page_measurement.MeasurementFailure):
  def __init__(self, name):
    super(MissingDisplayFrameRateError, self).__init__(
      'Missing display frame rate metrics: ' + name)

class SmoothnessController(object):
  def __init__(self):
    self._timeline_model = None
    self._tracing_timeline_data = None
    self._interaction = None

  def SetUp(self, page, tab):
    # FIXME: Remove webkit.console when blink.console lands in chromium and
    # the ref builds are updated. crbug.com/386847
    custom_categories = ['webkit.console', 'blink.console', 'benchmark']
    custom_categories += page.GetSyntheticDelayCategories()
    tab.browser.StartTracing(','.join(custom_categories), 60)
    if tab.browser.platform.IsRawDisplayFrameRateSupported():
      tab.browser.platform.StartRawDisplayFrameRateMeasurement()

  def Start(self, tab):
    # Start the smooth marker for all smooth actions.
    runner = action_runner.ActionRunner(tab)
    self._interaction = runner.BeginInteraction(
        RUN_SMOOTH_ACTIONS, is_smooth=True)

  def Stop(self, tab):
    # End the smooth marker for all smooth actions.
    self._interaction.End()
    # Stop tracing for smoothness metric.
    if tab.browser.platform.IsRawDisplayFrameRateSupported():
      tab.browser.platform.StopRawDisplayFrameRateMeasurement()
    self._tracing_timeline_data = tab.browser.StopTracing()
    self._timeline_model = TimelineModel(
      timeline_data=self._tracing_timeline_data)

  def AddResults(self, tab, results):
    # Add results of smoothness metric. This computes the smoothness metric for
    # the time ranges of gestures, if there is at least one, else the the time
    # ranges from the first action to the last action.

    renderer_thread = self._timeline_model.GetRendererThreadFromTabId(
        tab.id)
    run_smooth_actions_record = None
    smooth_records = []
    for event in renderer_thread.async_slices:
      if not tir_module.IsTimelineInteractionRecord(event.name):
        continue
      r = tir_module.TimelineInteractionRecord.FromAsyncEvent(event)
      if r.label == RUN_SMOOTH_ACTIONS:
        assert run_smooth_actions_record is None, (
          'SmoothnessController cannot issue more than 1 %s record' %
          RUN_SMOOTH_ACTIONS)
        run_smooth_actions_record = r
      elif r.is_smooth:
        smooth_records.append(
          smooth_gesture_util.GetAdjustedInteractionIfContainGesture(
            self._timeline_model, r))

    # If there is no other smooth records, we make measurements on time range
    # marked smoothness_controller itself.
    # TODO(nednguyen): when crbug.com/239179 is marked fixed, makes sure that
    # page sets are responsible for issueing the markers themselves.
    if len(smooth_records) == 0:
      if run_smooth_actions_record is None:
        sys.stderr.write('Raw tracing data:\n')
        sys.stderr.write(repr(self._tracing_timeline_data.EventData()))
        sys.stderr.write('\n')
        raise Exception('SmoothnessController failed to issue markers for the '
                        'whole interaction.')
      else:
        smooth_records = [run_smooth_actions_record]

    # Create an interaction_record for this legacy measurement. Since we don't
    # wrap the results that are sent to smoothness metric, the label will
    # not be used.
    smoothness_metric = smoothness.SmoothnessMetric()
    smoothness_metric.AddResults(
      self._timeline_model, renderer_thread, smooth_records, results)
    if tab.browser.platform.IsRawDisplayFrameRateSupported():
      for r in tab.browser.platform.GetRawDisplayFrameRateMeasurements():
        if r.value is None:
          raise MissingDisplayFrameRateError(r.name)
        if isinstance(r.value, list):
          results.AddValue(list_of_scalar_values.ListOfScalarValues(
              results.current_page, r.name, r.unit, r.value,
              description=DESCRIPTIONS.get(r.name)))
        else:
          results.AddValue(scalar.ScalarValue(
              results.current_page, r.name, r.unit, r.value,
              description=DESCRIPTIONS.get(r.name)))

  def CleanUp(self, tab):
    if tab.browser.platform.IsRawDisplayFrameRateSupported():
      tab.browser.platform.StopRawDisplayFrameRateMeasurement()
    if tab.browser.is_tracing_running:
      tab.browser.StopTracing()
