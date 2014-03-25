# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from metrics import Metric

class LoadingMetric(Metric):
  """A metric for page loading time based entirely on window.performance"""

  def Start(self, page, tab):
    raise NotImplementedError()

  def Stop(self, page, tab):
    raise NotImplementedError()

  def AddResults(self, tab, results):
    load_timings = tab.EvaluateJavaScript('window.performance.timing')

    # NavigationStart relative markers in milliseconds.
    load_start = (
      float(load_timings['loadEventStart']) - load_timings['navigationStart'])
    results.Add('load_start', 'ms', load_start)

    dom_content_loaded_start = (
      float(load_timings['domContentLoadedEventStart']) -
      load_timings['navigationStart'])
    results.Add('dom_content_loaded_start', 'ms', dom_content_loaded_start)

    fetch_start = (
        float(load_timings['fetchStart']) - load_timings['navigationStart'])
    results.Add('fetch_start', 'ms', fetch_start, data_type='unimportant')

    request_start = (
        float(load_timings['requestStart']) - load_timings['navigationStart'])
    results.Add('request_start', 'ms', request_start, data_type='unimportant')

    # Phase measurements in milliseconds.
    domain_lookup_duration = (
        float(load_timings['domainLookupEnd']) -
        load_timings['domainLookupStart'])
    results.Add('domain_lookup_duration', 'ms', domain_lookup_duration,
                data_type='unimportant')

    connect_duration = (
        float(load_timings['connectEnd']) - load_timings['connectStart'])
    results.Add('connect_duration', 'ms', connect_duration,
                data_type='unimportant')

    request_duration = (
        float(load_timings['responseStart']) - load_timings['requestStart'])
    results.Add('request_duration', 'ms', request_duration,
                data_type='unimportant')

    response_duration = (
        float(load_timings['responseEnd']) - load_timings['responseStart'])
    results.Add('response_duration', 'ms', response_duration,
                data_type='unimportant')
