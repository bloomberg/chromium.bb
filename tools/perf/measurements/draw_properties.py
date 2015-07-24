# Copyright 2015 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from telemetry.page import page_test
from telemetry.timeline import model
from telemetry.timeline import tracing_category_filter
from telemetry.timeline import tracing_options
from telemetry.value import scalar


class DrawProperties(page_test.PageTest):
  def __init__(self):
    super(DrawProperties, self).__init__()

  def CustomizeBrowserOptions(self, options):
    options.AppendExtraBrowserArgs([
        '--enable-prefer-compositing-to-lcd-text',
    ])

  def WillNavigateToPage(self, page, tab):
    options = tracing_options.TracingOptions()
    options.enable_chrome_trace = True
    category_filter = tracing_category_filter.TracingCategoryFilter(
        'disabled-by-default-cc.debug.cdp-perf')
    tab.browser.platform.tracing_controller.Start(options, category_filter)

  def ComputeAverageOfDurations(self, timeline_model, name):
    events = timeline_model.GetAllEventsOfName(name)
    event_durations = [d.duration for d in events]
    assert event_durations, 'Failed to find durations'

    duration_sum = sum(event_durations)
    duration_count = len(event_durations)
    duration_avg = duration_sum / duration_count
    return duration_avg

  def ValidateAndMeasurePage(self, page, tab, results):
    timeline_data = tab.browser.platform.tracing_controller.Stop()
    timeline_model = model.TimelineModel(timeline_data)

    pt_avg = self.ComputeAverageOfDurations(
        timeline_model,
        'LayerTreeHostCommon::ComputeVisibleRectsWithPropertyTrees')

    results.AddValue(scalar.ScalarValue(
        results.current_page, 'PT_avg_cost', 'ms', pt_avg,
        description='Average time spent processing property trees'))

  def DidRunPage(self, platform):
    tracing_controller = platform.tracing_controller
    if tracing_controller.is_tracing_running:
      tracing_controller.Stop()
