# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os

from telemetry import benchmark
from telemetry import page as page_module
from telemetry.core import util
from telemetry.page import page_set
from telemetry.page import page_test
from telemetry.value import list_of_scalar_values


BLINK_PERF_BASE_DIR = os.path.join(util.GetChromiumSrcDir(),
                                   'third_party', 'WebKit', 'PerformanceTests')
SKIPPED_FILE = os.path.join(BLINK_PERF_BASE_DIR, 'Skipped')


def CreatePageSetFromPath(path, skipped_file):
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
  ps = page_set.PageSet(file_path=os.getcwd()+os.sep,
                        serving_dirs=serving_dirs)
  for url in page_urls:
    ps.AddUserStory(page_module.Page(url, ps, ps.base_dir))
  return ps


class _BlinkPerfMeasurement(page_test.PageTest):
  """Tuns a blink performance test and reports the results."""
  def __init__(self):
    super(_BlinkPerfMeasurement, self).__init__()
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
    if 'content-shell' in options.browser_type:
      options.AppendExtraBrowserArgs('--expose-internals-for-testing')

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


class _BlinkPerfFullFrameMeasurement(_BlinkPerfMeasurement):
  def __init__(self):
    super(_BlinkPerfFullFrameMeasurement, self).__init__()
    self._blink_perf_js += '\nwindow.fullFrameMeasurement = true;'

  def CustomizeBrowserOptions(self, options):
    super(_BlinkPerfFullFrameMeasurement, self).CustomizeBrowserOptions(
        options)
    # Full layout measurement needs content_shell with internals testing API.
    assert 'content-shell' in options.browser_type
    options.AppendExtraBrowserArgs(['--expose-internals-for-testing'])


class BlinkPerfAnimation(benchmark.Benchmark):
  tag = 'animation'
  test = _BlinkPerfMeasurement

  @classmethod
  def Name(cls):
    return 'blink_perf.animation'

  def CreatePageSet(self, options):
    path = os.path.join(BLINK_PERF_BASE_DIR, 'Animation')
    return CreatePageSetFromPath(path, SKIPPED_FILE)


class BlinkPerfBindings(benchmark.Benchmark):
  tag = 'bindings'
  test = _BlinkPerfMeasurement

  @classmethod
  def Name(cls):
    return 'blink_perf.bindings'

  def CreatePageSet(self, options):
    path = os.path.join(BLINK_PERF_BASE_DIR, 'Bindings')
    return CreatePageSetFromPath(path, SKIPPED_FILE)


@benchmark.Enabled('content-shell')
class BlinkPerfBlinkGC(benchmark.Benchmark):
  tag = 'blink_gc'
  test = _BlinkPerfMeasurement

  @classmethod
  def Name(cls):
    return 'blink_perf.blink_gc'

  def CreatePageSet(self, options):
    path = os.path.join(BLINK_PERF_BASE_DIR, 'BlinkGC')
    return CreatePageSetFromPath(path, SKIPPED_FILE)


class BlinkPerfCSS(benchmark.Benchmark):
  tag = 'css'
  test = _BlinkPerfMeasurement

  @classmethod
  def Name(cls):
    return 'blink_perf.css'

  def CreatePageSet(self, options):
    path = os.path.join(BLINK_PERF_BASE_DIR, 'CSS')
    return CreatePageSetFromPath(path, SKIPPED_FILE)


@benchmark.Disabled('win')  # crbug.com/455796
class BlinkPerfCanvas(benchmark.Benchmark):
  tag = 'canvas'
  test = _BlinkPerfMeasurement

  @classmethod
  def Name(cls):
    return 'blink_perf.canvas'

  def CreatePageSet(self, options):
    path = os.path.join(BLINK_PERF_BASE_DIR, 'Canvas')
    return CreatePageSetFromPath(path, SKIPPED_FILE)


class BlinkPerfDOM(benchmark.Benchmark):
  tag = 'dom'
  test = _BlinkPerfMeasurement

  @classmethod
  def Name(cls):
    return 'blink_perf.dom'

  def CreatePageSet(self, options):
    path = os.path.join(BLINK_PERF_BASE_DIR, 'DOM')
    return CreatePageSetFromPath(path, SKIPPED_FILE)


class BlinkPerfEvents(benchmark.Benchmark):
  tag = 'events'
  test = _BlinkPerfMeasurement

  @classmethod
  def Name(cls):
    return 'blink_perf.events'

  def CreatePageSet(self, options):
    path = os.path.join(BLINK_PERF_BASE_DIR, 'Events')
    return CreatePageSetFromPath(path, SKIPPED_FILE)


@benchmark.Disabled('win8')  # http://crbug.com/462350
class BlinkPerfLayout(benchmark.Benchmark):
  tag = 'layout'
  test = _BlinkPerfMeasurement

  @classmethod
  def Name(cls):
    return 'blink_perf.layout'

  def CreatePageSet(self, options):
    path = os.path.join(BLINK_PERF_BASE_DIR, 'Layout')
    return CreatePageSetFromPath(path, SKIPPED_FILE)


@benchmark.Enabled('content-shell')
class BlinkPerfLayoutFullLayout(BlinkPerfLayout):
  tag = 'layout_full_frame'
  test = _BlinkPerfFullFrameMeasurement

  @classmethod
  def Name(cls):
    return 'blink_perf.layout_full_frame'


class BlinkPerfMutation(benchmark.Benchmark):
  tag = 'mutation'
  test = _BlinkPerfMeasurement

  @classmethod
  def Name(cls):
    return 'blink_perf.mutation'

  def CreatePageSet(self, options):
    path = os.path.join(BLINK_PERF_BASE_DIR, 'Mutation')
    return CreatePageSetFromPath(path, SKIPPED_FILE)


class BlinkPerfParser(benchmark.Benchmark):
  tag = 'parser'
  test = _BlinkPerfMeasurement

  @classmethod
  def Name(cls):
    return 'blink_perf.parser'

  def CreatePageSet(self, options):
    path = os.path.join(BLINK_PERF_BASE_DIR, 'Parser')
    return CreatePageSetFromPath(path, SKIPPED_FILE)


class BlinkPerfSVG(benchmark.Benchmark):
  tag = 'svg'
  test = _BlinkPerfMeasurement

  @classmethod
  def Name(cls):
    return 'blink_perf.svg'

  def CreatePageSet(self, options):
    path = os.path.join(BLINK_PERF_BASE_DIR, 'SVG')
    return CreatePageSetFromPath(path, SKIPPED_FILE)


@benchmark.Enabled('content-shell')
class BlinkPerfSVGFullLayout(BlinkPerfSVG):
  tag = 'svg_full_frame'
  test = _BlinkPerfFullFrameMeasurement

  @classmethod
  def Name(cls):
    return 'blink_perf.svg_full_frame'


class BlinkPerfShadowDOM(benchmark.Benchmark):
  tag = 'shadow_dom'
  test = _BlinkPerfMeasurement

  @classmethod
  def Name(cls):
    return 'blink_perf.shadow_dom'

  def CreatePageSet(self, options):
    path = os.path.join(BLINK_PERF_BASE_DIR, 'ShadowDOM')
    return CreatePageSetFromPath(path, SKIPPED_FILE)


# This benchmark is for local testing, doesn't need to run on bots.
@benchmark.Disabled()
class BlinkPerfXMLHttpRequest(benchmark.Benchmark):
  tag = 'xml_http_request'
  test = _BlinkPerfMeasurement

  @classmethod
  def Name(cls):
    return 'blink_perf.xml_http_request'

  def CreatePageSet(self, options):
    path = os.path.join(BLINK_PERF_BASE_DIR, 'XMLHttpRequest')
    return CreatePageSetFromPath(path, SKIPPED_FILE)
