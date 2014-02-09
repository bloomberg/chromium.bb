# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Runs a Google Maps performance test.
Rerforms several common navigation actions on the map (pan, zoom, rotate)"""

import os
import re

from telemetry import test
from telemetry.core import util
from telemetry.page import page_measurement
from telemetry.page import page_set


class MapsMeasurement(page_measurement.PageMeasurement):
  def MeasurePage(self, page, tab, results):
    js_get_results = 'document.getElementsByTagName("pre")[0].innerText'
    test_results = tab.EvaluateJavaScript(js_get_results)

    total = re.search('total=([0-9]+)', test_results).group(1)
    render = re.search('render=([0-9.]+),([0-9.]+)', test_results).group(2)
    results.Add('total_time', 'ms', total)
    results.Add('render_mean_time', 'ms', render)

@test.Disabled
class MapsBenchmark(test.Test):
  """Basic Google Maps benchmarks."""
  test = MapsMeasurement

  def CreatePageSet(self, options):
    page_set_path = os.path.join(
        util.GetChromiumSrcDir(), 'tools', 'perf', 'page_sets')
    page_set_dict = {
      'archive_data_file': 'data/maps.json',
      'make_javascript_deterministic': False,
      'pages': [
        {
          'url': 'http://localhost:10020/tracker.html',
          'navigate_steps' : [
            { 'action': 'navigate' },
            { 'action': 'wait', 'javascript': 'window.testDone' }
          ]
        }
      ]
    }

    return page_set.PageSet.FromDict(page_set_dict, page_set_path)

class MapsNoVsync(MapsBenchmark):
  """Runs the Google Maps benchmark with Vsync disabled"""
  tag = 'novsync'

  def CustomizeBrowserOptions(self, options):
    options.AppendExtraBrowserArgs('--disable-gpu-vsync')
