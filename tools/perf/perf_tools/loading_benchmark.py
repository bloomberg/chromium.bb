# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import collections

from telemetry import multi_page_benchmark

class LoadingBenchmark(multi_page_benchmark.MultiPageBenchmark):
  @property
  def results_are_the_same_on_every_page(self):
    return False

  def WillNavigateToPage(self, page, tab):
    tab.StartTimelineRecording()

  def MeasurePage(self, page, tab, results):
    # In current telemetry tests, all tests wait for DocumentComplete state.
    #
    # TODO(nduca): when crbug.com/168431 is fixed, modify the page sets to
    # recognize loading as a toplevel action.
    tab.StopTimelineRecording()

    events = tab.timeline_model.GetAllEvents()

    events_by_name = collections.defaultdict(list)
    for e in events:
      events_by_name[e.name].append(e)

    for key, group in events_by_name.items():
      times = [e.self_time_ms for e in group]
      total = sum(times)
      biggest_jank = max(times)
      results.Add(key, 'ms', total)
      results.Add(key + '_max', 'ms', biggest_jank)
      results.Add(key + '_avg', 'ms', total / len(times))
