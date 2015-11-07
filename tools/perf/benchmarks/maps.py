# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Runs a Google Maps performance test.
Rerforms several common navigation actions on the map (pan, zoom, rotate)"""

import re

from core import path_util
from core import perf_benchmark

from telemetry import benchmark
from telemetry.page import page as page_module
from telemetry.page import page_test
from telemetry import story
from telemetry.value import scalar


class _MapsMeasurement(page_test.PageTest):
  def __init__(self):
    super(_MapsMeasurement, self).__init__()

  def ValidateAndMeasurePage(self, page, tab, results):
    js_get_results = 'document.getElementsByTagName("pre")[0].innerText'
    test_results = tab.EvaluateJavaScript(js_get_results)

    total = re.search('total=([0-9]+)', test_results).group(1)
    render = re.search('render=([0-9.]+),([0-9.]+)', test_results).group(2)
    results.AddValue(scalar.ScalarValue(
        results.current_page, 'total_time', 'ms', int(total)))
    results.AddValue(scalar.ScalarValue(
        results.current_page, 'render_mean_time', 'ms', float(render)))

class MapsPage(page_module.Page):
  def __init__(self, page_set, base_dir):
    super(MapsPage, self).__init__(
        url='http://localhost:10020/tracker.html',
        page_set=page_set,
        base_dir=base_dir,
        make_javascript_deterministic=False)

  def RunNavigateSteps(self, action_runner):
    super(MapsPage, self).RunNavigateSteps(action_runner)
    action_runner.WaitForJavaScriptCondition('window.testDone')


@benchmark.Disabled('all')
class MapsBenchmark(perf_benchmark.PerfBenchmark):
  """Basic Google Maps benchmarks."""
  test = _MapsMeasurement

  @classmethod
  def Name(cls):
    return 'maps'

  def CreateStorySet(self, options):
    ps = story.StorySet(archive_data_file='data/maps.json',
                        base_dir=path_util.GetStorySetsDir(),
                        cloud_storage_bucket=story.PUBLIC_BUCKET)
    ps.AddStory(MapsPage(ps, ps.base_dir))
    return ps

class MapsNoVsync(MapsBenchmark):
  """Runs the Google Maps benchmark with Vsync disabled"""
  tag = 'novsync'

  @classmethod
  def Name(cls):
    return 'maps.novsync'

  def SetExtraBrowserOptions(self, options):
    options.AppendExtraBrowserArgs('--disable-gpu-vsync')
