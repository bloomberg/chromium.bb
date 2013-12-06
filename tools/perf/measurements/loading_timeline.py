# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
from metrics import timeline
from metrics import loading
from telemetry.page import page_measurement

class LoadingTimeline(page_measurement.PageMeasurement):
  def __init__(self, *args, **kwargs):
    super(LoadingTimeline, self).__init__(*args, **kwargs)
    self._timeline_metric = timeline.LoadTimesTimelineMetric(
        timeline.TIMELINE_MODE, 'thread 0')

  @property
  def results_are_the_same_on_every_page(self):
    return False

  def WillNavigateToPage(self, page, tab):
    self._timeline_metric.Start(page, tab)

  def MeasurePage(self, page, tab, results):
    # In current telemetry tests, all tests wait for DocumentComplete state,
    # but we need to wait for the load event.
    tab.WaitForJavaScriptExpression('performance.timing.loadEventStart', 300)

    # TODO(nduca): when crbug.com/168431 is fixed, modify the page sets to
    # recognize loading as a toplevel action.
    self._timeline_metric.Stop(page, tab)

    loading.LoadingMetric().AddResults(tab, results)
    self._timeline_metric.AddResults(tab, results)
