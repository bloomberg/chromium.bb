# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Media measurement class gathers media related metrics on a page set.

Media metrics recorded are controlled by metrics/media.js.  At the end of the
test each metrics for every media element in the page are reported.
"""
import metrics.media

from telemetry.page import page_measurement


class Media(page_measurement.PageMeasurement):
  """Provide general video and audio metrics."""

  def __init__(self):
    super(Media, self).__init__('media_metrics')
    self.metrics = None

  def results_are_the_same_on_every_page(self):
    """Results can vary from page to page based on media events taking place."""
    return False

  def DidNavigateToPage(self, page, tab):
    """Override to do operations right after the page is navigated."""
    self.metrics = metrics.media.MediaMetrics(tab)
    self.metrics.Start()

  def MeasurePage(self, page, tab, results):
    """Measure the page's performance."""
    self.metrics.StopAndGetResults(results)
