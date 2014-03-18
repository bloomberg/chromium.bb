# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import collections

from metrics import timeline
from metrics.timeline import TimelineMetric

class V8Metric(TimelineMetric):
  """A metric for measuring V8 functionalities."""

  def __init__(self):
    super(V8Metric, self).__init__(timeline.TRACING_MODE)
    self.report_main_thread_only = False

  # Optional argument chart_name is not in base class Metric.
  # pylint: disable=W0221
  def AddResults(self, _, results, chart_name=None):
    assert self._model
    events_by_name = collections.defaultdict(list)
    for thread in self.renderer_process.threads.itervalues():
      events = thread.all_slices

      for e in events:
        if e.name.startswith('V8'):
          events_by_name[e.name].append(e)

      for event_name, event_group in events_by_name.iteritems():
        times = [event.self_time for event in event_group]
        total = sum(times)

        # Results objects cannot contain the '.' character, so remove that here.
        sanitized_event_name = event_name.replace('.', '_')

        results.Add(sanitized_event_name, 'ms', total, chart_name=chart_name)
