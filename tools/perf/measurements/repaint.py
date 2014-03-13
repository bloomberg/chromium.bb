# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from metrics import smoothness
from metrics import timeline_interaction_record as tir_module
from telemetry.core.timeline.model import TimelineModel
from telemetry.page import page_measurement

import telemetry.core.timeline.bounds as timeline_bounds


class Repaint(page_measurement.PageMeasurement):
  def __init__(self):
    super(Repaint, self).__init__('repaint', False)
    self._smoothness_metric = None
    self._timeline_model = None
    self._actions = []

  def CanRunForPage(self, page):
    return hasattr(page, 'repaint')

  def WillRunActions(self, page, tab):
    tab.WaitForDocumentReadyStateToBeComplete()
    # Rasterize only what's visible.
    tab.ExecuteJavaScript(
        'chrome.gpuBenchmarking.setRasterizeOnlyVisibleContent();')
    self._smoothness_metric = smoothness.SmoothnessMetric()
    # Start tracing for smoothness metric.
    custom_categories = 'webkit.console,benchmark'
    tab.browser.StartTracing(custom_categories, 60)

  def DidRunAction(self, page, tab, action):
    self._actions.append(action)

  def DidRunActions(self, page, tab):
    # Stop tracing for smoothness metric.
    tracing_timeline_data = tab.browser.StopTracing()
    self._timeline_model = TimelineModel(timeline_data=tracing_timeline_data)

  def MeasurePage(self, page, tab, results):
    # Add results of smoothness metric. This computes the smoothness metric for
    # the time range between the first action starts and the last action ends.
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
