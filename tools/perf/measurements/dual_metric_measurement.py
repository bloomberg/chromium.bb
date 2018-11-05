# Copyright 2018 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from telemetry.web_perf import story_test
from telemetry.web_perf import timeline_based_measurement


class DualMetricMeasurement(story_test.StoryTest):
  """Test class for a benchmark that aggregates all metrics.

    Assumes both javascript as well as tracing metrics might be defined.

    All pages associated with this measurement must implement
    GetJavascriptMetricValues()
  """
  def __init__(self, tbm_options):
    super(DualMetricMeasurement, self).__init__()
    # Only enable tracing if metrics have been specified.
    if tbm_options.GetTimelineBasedMetrics():
      self._tbm_test = timeline_based_measurement.TimelineBasedMeasurement(
          tbm_options)
      self._enable_tracing = True
    else:
      self._enable_tracing = False

  def WillRunStory(self, platform):
    if self._enable_tracing:
      self._tbm_test.WillRunStory(platform)

  def Measure(self, platform, results):
    for value in results.current_page.GetJavascriptMetricValues():
      results.AddValue(value)
    # This call is necessary to convert the current ScalarValues to
    # histograms before more histograms are added.  If we don't,
    # when histograms get added by TBM2 page_test_results will see those and
    # not convert any existing values because it assumes they are already
    # converted.  Therefore, so the javascript metrics don't get dropped, we
    # have to convert them first.
    results.PopulateHistogramSet()
    if self._enable_tracing:
      self._tbm_test.Measure(platform, results)

  def DidRunStory(self, platform, results):
    if self._enable_tracing:
      self._tbm_test.DidRunStory(platform, results)

