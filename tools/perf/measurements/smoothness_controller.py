# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
from telemetry.core.platform import tracing_category_filter
from telemetry.core.platform import tracing_options
from telemetry.timeline.model import TimelineModel
from telemetry.value import trace
from telemetry.web_perf.metrics import smoothness
from telemetry.web_perf import smooth_gesture_util
from telemetry.web_perf import timeline_interaction_record as tir_module

from telemetry.page import page_test


class SmoothnessController(object):
  def __init__(self):
    self._timeline_model = None
    self._trace_data = None
    self._interaction = None
    self._surface_flinger_trace_data = None

  def Start(self, page, tab):
    # FIXME: Remove webkit.console when blink.console lands in chromium and
    # the ref builds are updated. crbug.com/386847
    custom_categories = ['webkit.console', 'blink.console', 'benchmark']
    custom_categories += page.GetSyntheticDelayCategories()
    category_filter = tracing_category_filter.TracingCategoryFilter()
    for c in custom_categories:
      category_filter.AddIncludedCategory(c)
    options = tracing_options.TracingOptions()
    options.enable_chrome_trace = True
    options.enable_platform_display_trace = True
    tab.browser.platform.tracing_controller.Start(options, category_filter, 60)

  def Stop(self, tab):
    self._trace_data = tab.browser.platform.tracing_controller.Stop()
    self._timeline_model = TimelineModel(self._trace_data)

  def AddResults(self, tab, results):
    # Add results of smoothness metric. This computes the smoothness metric for
    # the time ranges of gestures, if there is at least one, else the the time
    # ranges from the first action to the last action.
    results.AddValue(trace.TraceValue(
        results.current_page, self._trace_data))
    renderer_thread = self._timeline_model.GetRendererThreadFromTabId(
        tab.id)
    smooth_records = []
    for event in renderer_thread.async_slices:
      if not tir_module.IsTimelineInteractionRecord(event.name):
        continue
      r = tir_module.TimelineInteractionRecord.FromAsyncEvent(event)
      smooth_records.append(
        smooth_gesture_util.GetAdjustedInteractionIfContainGesture(
          self._timeline_model, r))

    # If there is no other smooth records, we make measurements on time range
    # marked smoothness_controller itself.
    # TODO(nednguyen): when crbug.com/239179 is marked fixed, makes sure that
    # page sets are responsible for issueing the markers themselves.
    if len(smooth_records) == 0:
      raise page_test.Failure('Page failed to issue any markers.')

    # Check to make sure all smooth records have same label and repeatable if
    # there are more than one.
    need_repeatable_flag = len(smooth_records) > 1
    record_label = smooth_records[0].label
    for r in smooth_records:
      if r.label != record_label:
        raise page_test.Failure(
            'SmoothController does not support multiple interactions with '
            'different label. Interactions: %s' % repr(smooth_records))
      if need_repeatable_flag and not r.repeatable:
        raise page_test.Failure(
            'If there are more than one interaction record, each interaction '
            'must has repeatable flag. Interactions: %s' % repr(smooth_records))

    # Create an interaction_record for this legacy measurement. Since we don't
    # wrap the results that are sent to smoothness metric, the label will
    # not be used.
    smoothness_metric = smoothness.SmoothnessMetric()
    smoothness_metric.AddResults(
        self._timeline_model, renderer_thread, smooth_records, results)

  def CleanUp(self, tab):
    if tab.browser.platform.tracing_controller.is_tracing_running:
      tab.browser.platform.tracing_controller.Stop()
