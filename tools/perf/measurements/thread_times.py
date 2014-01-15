# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from metrics import timeline
from telemetry.page import page_measurement

class ThreadTimes(page_measurement.PageMeasurement):
  def __init__(self):
    super(ThreadTimes, self).__init__('smoothness')
    self._metric = timeline.ThreadTimesTimelineMetric()

  def AddCommandLineOptions(self, parser):
    parser.add_option('--report-renderer-main-details', action='store_true',
                      help='Report details on the render main thread.')

  def CanRunForPage(self, page):
    return hasattr(page, 'smoothness')

  @property
  def results_are_the_same_on_every_page(self):
    return False

  def WillRunActions(self, page, tab):
    self._metric.Start(page, tab)
    self._metric.report_renderer_main_details = \
      self.options.report_renderer_main_details

  def DidRunActions(self, page, tab):
    self._metric.Stop(page, tab)

  def MeasurePage(self, page, tab, results):
    self._metric.AddResults(tab, results)
