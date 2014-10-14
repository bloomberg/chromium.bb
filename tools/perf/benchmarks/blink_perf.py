# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os

from telemetry import benchmark
from telemetry.core import util
from telemetry.page import page_set
from telemetry.page import page_test
from telemetry.value import list_of_scalar_values


BLINK_PERF_BASE_DIR = os.path.join(util.GetChromiumSrcDir(),
                                   'third_party', 'WebKit', 'PerformanceTests')
SKIPPED_FILE = os.path.join(BLINK_PERF_BASE_DIR, 'Skipped')


def _CreatePageSetFromPath(path, skipped_file):
  assert os.path.exists(path)

  page_urls = []
  serving_dirs = set()

  def _AddPage(path):
    if not path.endswith('.html'):
      return
    if '../' in open(path, 'r').read():
      # If the page looks like it references its parent dir, include it.
      serving_dirs.add(os.path.dirname(os.path.dirname(path)))
    page_urls.append('file://' + path.replace('\\', '/'))

  def _AddDir(dir_path, skipped):
    for candidate_path in os.listdir(dir_path):
      if candidate_path == 'resources':
        continue
      candidate_path = os.path.join(dir_path, candidate_path)
      if candidate_path.startswith(skipped):
        continue
      if os.path.isdir(candidate_path):
        _AddDir(candidate_path, skipped)
      else:
        _AddPage(candidate_path)

  if os.path.isdir(path):
    skipped = []
    if os.path.exists(skipped_file):
      for line in open(skipped_file, 'r').readlines():
        line = line.strip()
        if line and not line.startswith('#'):
          skipped_path = os.path.join(os.path.dirname(skipped_file), line)
          skipped.append(skipped_path.replace('/', os.sep))
    _AddDir(path, tuple(skipped))
  else:
    _AddPage(path)
  ps = page_set.PageSet(file_path=os.getcwd()+os.sep, serving_dirs=serving_dirs)
  for url in page_urls:
    ps.AddPageWithDefaultRunNavigate(url)
  return ps


class _BlinkPerfMeasurement(page_test.PageTest):
  """Tuns a blink performance test and reports the results."""
  def __init__(self):
    super(_BlinkPerfMeasurement, self).__init__('')
    with open(os.path.join(os.path.dirname(__file__),
                           'blink_perf.js'), 'r') as f:
      self._blink_perf_js = f.read()

  def WillNavigateToPage(self, page, tab):
    page.script_to_evaluate_on_commit = self._blink_perf_js

  def CustomizeBrowserOptions(self, options):
    options.AppendExtraBrowserArgs([
        '--js-flags=--expose_gc',
        '--enable-experimental-web-platform-features',
        '--disable-gesture-requirement-for-media-playback'
    ])

  def ValidateAndMeasurePage(self, page, tab, results):
    tab.WaitForJavaScriptExpression('testRunner.isDone', 600)

    log = tab.EvaluateJavaScript('document.getElementById("log").innerHTML')

    for line in log.splitlines():
      if not line.startswith('values '):
        continue
      parts = line.split()
      values = [float(v.replace(',', '')) for v in parts[1:-1]]
      units = parts[-1]
      metric = page.display_name.split('.')[0].replace('/', '_')
      results.AddValue(list_of_scalar_values.ListOfScalarValues(
          results.current_page, metric, units, values))

      break

    print log


class BlinkPerfAnimation(benchmark.Benchmark):
  tag = 'animation'
  test = _BlinkPerfMeasurement

  def CreatePageSet(self, options):
    path = os.path.join(BLINK_PERF_BASE_DIR, 'Animation')
    return _CreatePageSetFromPath(path, SKIPPED_FILE)


class BlinkPerfBindings(benchmark.Benchmark):
  tag = 'bindings'
  test = _BlinkPerfMeasurement

  def CreatePageSet(self, options):
    path = os.path.join(BLINK_PERF_BASE_DIR, 'Bindings')
    return _CreatePageSetFromPath(path, SKIPPED_FILE)


class BlinkPerfCSS(benchmark.Benchmark):
  tag = 'css'
  test = _BlinkPerfMeasurement

  def CreatePageSet(self, options):
    path = os.path.join(BLINK_PERF_BASE_DIR, 'CSS')
    return _CreatePageSetFromPath(path, SKIPPED_FILE)


class BlinkPerfCanvas(benchmark.Benchmark):
  tag = 'canvas'
  test = _BlinkPerfMeasurement

  def CreatePageSet(self, options):
    path = os.path.join(BLINK_PERF_BASE_DIR, 'Canvas')
    return _CreatePageSetFromPath(path, SKIPPED_FILE)


class BlinkPerfDOM(benchmark.Benchmark):
  tag = 'dom'
  test = _BlinkPerfMeasurement

  def CreatePageSet(self, options):
    path = os.path.join(BLINK_PERF_BASE_DIR, 'DOM')
    return _CreatePageSetFromPath(path, SKIPPED_FILE)


class BlinkPerfEvents(benchmark.Benchmark):
  tag = 'events'
  test = _BlinkPerfMeasurement

  def CreatePageSet(self, options):
    path = os.path.join(BLINK_PERF_BASE_DIR, 'Events')
    return _CreatePageSetFromPath(path, SKIPPED_FILE)


class BlinkPerfInteractive(benchmark.Benchmark):
  tag = 'interactive'
  test = _BlinkPerfMeasurement

  def CreatePageSet(self, options):
    path = os.path.join(BLINK_PERF_BASE_DIR, 'Interactive')
    return _CreatePageSetFromPath(path, SKIPPED_FILE)


class BlinkPerfLayout(benchmark.Benchmark):
  tag = 'layout'
  test = _BlinkPerfMeasurement

  def CreatePageSet(self, options):
    path = os.path.join(BLINK_PERF_BASE_DIR, 'Layout')
    return _CreatePageSetFromPath(path, SKIPPED_FILE)


class BlinkPerfMutation(benchmark.Benchmark):
  tag = 'mutation'
  test = _BlinkPerfMeasurement

  def CreatePageSet(self, options):
    path = os.path.join(BLINK_PERF_BASE_DIR, 'Mutation')
    return _CreatePageSetFromPath(path, SKIPPED_FILE)


class BlinkPerfParser(benchmark.Benchmark):
  tag = 'parser'
  test = _BlinkPerfMeasurement

  def CreatePageSet(self, options):
    path = os.path.join(BLINK_PERF_BASE_DIR, 'Parser')
    return _CreatePageSetFromPath(path, SKIPPED_FILE)


class BlinkPerfSVG(benchmark.Benchmark):
  tag = 'svg'
  test = _BlinkPerfMeasurement

  def CreatePageSet(self, options):
    path = os.path.join(BLINK_PERF_BASE_DIR, 'SVG')
    return _CreatePageSetFromPath(path, SKIPPED_FILE)


class BlinkPerfShadowDOM(benchmark.Benchmark):
  tag = 'shadow_dom'
  test = _BlinkPerfMeasurement

  def CreatePageSet(self, options):
    path = os.path.join(BLINK_PERF_BASE_DIR, 'ShadowDOM')
    return _CreatePageSetFromPath(path, SKIPPED_FILE)


class BlinkPerfXMLHttpRequest(benchmark.Benchmark):
  tag = 'xml_http_request'
  test = _BlinkPerfMeasurement

  def CreatePageSet(self, options):
    path = os.path.join(BLINK_PERF_BASE_DIR, 'XMLHttpRequest')
    return _CreatePageSetFromPath(path, SKIPPED_FILE)
