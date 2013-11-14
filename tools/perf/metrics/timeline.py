# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
import collections

from metrics import Metric

TRACING_MODE = 'tracing-mode'
TIMELINE_MODE = 'timeline-mode'

class TimelineMetric(Metric):
  def __init__(self, mode, thread_filter = None):
    ''' Initializes a TimelineMetric object.

    mode: TRACING_MODE or TIMELINE_MODE
    thread_filter: list of thread names to include.
        Empty list or None includes all threads.
        In TIMELINE_MODE, only 'thread 0' is available, which is the renderer
        main thread.
        In TRACING_MODE, the following renderer process threads are available:
        'CrRendererMain', 'Chrome_ChildIOThread', and 'Compositor'.
        CompositorRasterWorker threads are available with impl-side painting
        enabled.
    '''
    assert mode in (TRACING_MODE, TIMELINE_MODE)
    super(TimelineMetric, self).__init__()
    self._mode = mode
    self._thread_filter = thread_filter
    self._model = None
    self._renderer_process = None

  def Start(self, page, tab):
    self._model = None
    self._renderer_process = None

    if self._mode == TRACING_MODE:
      if not tab.browser.supports_tracing:
        raise Exception('Not supported')
      tab.browser.StartTracing()
    else:
      assert self._mode == TIMELINE_MODE
      tab.StartTimelineRecording()

  def Stop(self, page, tab):
    if self._mode == TRACING_MODE:
      trace_result = tab.browser.StopTracing()
      self._model = trace_result.AsTimelineModel()
      self._renderer_process = self._model.GetRendererProcessFromTab(tab)
    else:
      tab.StopTimelineRecording()
      self._model = tab.timeline_model
      self._renderer_process = self._model.GetAllProcesses()[0]

  def AddResults(self, tab, results):
    assert self._model

    events_by_name = collections.defaultdict(list)

    for thread in self._renderer_process.threads.itervalues():

      if self._thread_filter and not thread.name in self._thread_filter:
        continue

      thread_name = thread.name.replace('/','_')
      events = thread.all_slices

      for e in events:
        events_by_name[e.name].append(e)

      for event_name, event_group in events_by_name.iteritems():
        times = [event.self_time for event in event_group]
        total = sum(times)
        biggest_jank = max(times)
        full_name = thread_name + '|' + event_name
        results.Add(full_name, 'ms', total)
        results.Add(full_name + '_max', 'ms', biggest_jank)
        results.Add(full_name + '_avg', 'ms', total / len(times))

    for counter_name, counter in self._renderer_process.counters.iteritems():
      total = sum(counter.totals)
      results.Add(counter_name, 'count', total)
      results.Add(counter_name + '_avg', 'count', total / len(counter.totals))
