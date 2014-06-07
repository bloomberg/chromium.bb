# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
from measurements import timeline_controller
from metrics import loading
from metrics import timeline
from telemetry.page import page_measurement
from telemetry.web_perf import timeline_interaction_record as tir_module

class LoadingTrace(page_measurement.PageMeasurement):
  def __init__(self, *args, **kwargs):
    super(LoadingTrace, self).__init__(*args, **kwargs)
    self._timeline_controller = timeline_controller.TimelineController()

  @property
  def results_are_the_same_on_every_page(self):
    return False

  def WillNavigateToPage(self, page, tab):
    self._timeline_controller.Start(page, tab)

  def MeasurePage(self, page, tab, results):
    # In current telemetry tests, all tests wait for DocumentComplete state,
    # but we need to wait for the load event.
    tab.WaitForJavaScriptExpression('performance.timing.loadEventStart', 300)

    # TODO(nduca): when crbug.com/168431 is fixed, modify the page sets to
    # recognize loading as a toplevel action.
    self._timeline_controller.Stop(tab)

    loading.LoadingMetric().AddResults(tab, results)
    timeline_metric = timeline.LoadTimesTimelineMetric()
    renderer_thread = \
        self._timeline_controller.model.GetRendererThreadFromTabId(tab.id)
    record = tir_module.TimelineInteractionRecord(
      "loading_trace_interaction", 0, float('inf'))
    timeline_metric.AddResults(
      self._timeline_controller.model,
      renderer_thread,
      [record],
      results)

  def CleanUpAfterPage(self, _, tab):
    self._timeline_controller.CleanUp(tab)
