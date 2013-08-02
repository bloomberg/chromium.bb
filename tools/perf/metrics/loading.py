# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

def AddResultsForTab(tab, results):
  load_timings = tab.EvaluateJavaScript('window.performance.timing')
  load_time_ms = (
    float(load_timings['loadEventStart']) -
    load_timings['navigationStart'])
  dom_content_loaded_time_ms = (
    float(load_timings['domContentLoadedEventStart']) -
    load_timings['navigationStart'])
  results.Add('load_time', 'ms', load_time_ms)
  results.Add('dom_content_loaded_time', 'ms',
              dom_content_loaded_time_ms)
