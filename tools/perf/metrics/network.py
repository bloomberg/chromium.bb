# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import json

from metrics import Metric

_COUNTER_TO_RESULTS = {
    'tcp.read_bytes':               ('tcp_read_bytes',    'bytes'),
    'tcp.write_bytes':              ('tcp_write_bytes',   'bytes'),
    'HttpNetworkTransaction.Count': ('num_requests',      'count'),
    'tcp.connect':                  ('num_connects',      'count'),
    'spdy.sessions':                ('num_spdy_sessions', 'count'),
    }

class NetworkMetric(Metric):
  """A metric for network information."""

  def __init__(self):
    super(NetworkMetric, self).__init__()
    self._results = None

  def _GetCounters(self, tab):
    return tab.EvaluateJavaScript("""
(function(counters) {
  var results = {};
  if (!window.chrome || !window.chrome.benchmarking)
    return results;
  for (var i = 0; i < counters.length; i++)
    results[counters[i]] = chrome.benchmarking.counter(counters[i]);
  return results;
})(%s);
""" % json.dumps(_COUNTER_TO_RESULTS.keys()))

  def Start(self, page, tab):
    assert not self._results, 'Must AddResults() before calling Start() again.'
    self._results = self._GetCounters(tab)

  def Stop(self, page, tab):
    assert self._results, 'Must Start() before Stop().'
    results = self._GetCounters(tab)
    for counter in self._results:
      self._results[counter] = results[counter] - self._results[counter]

  def AddResults(self, tab, results):
    assert self._results, 'Must Start() and Stop() before AddResults().'
    for counter, trace_unit in _COUNTER_TO_RESULTS.iteritems():
      if counter not in self._results:
        continue
      results.Add(trace_unit[0], trace_unit[1], self._results[counter],
                  data_type='unimportant')
    self._results = None
