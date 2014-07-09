# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Runs Browsermark CSS, DOM, WebGL, JS, resize and page load benchmarks.

Browsermark benchmark suite have five test groups:
a) CSS group: measures your browsers 2D and 3D performance, and finally executes
  CSS Crunch test
b) DOM group: measures variety of areas, like how well your browser traverse in
  Document Object Model Tree or how fast your browser can create dynamic content
c) General group: measures areas like resize and page load times
d) Graphics group: tests browsers Graphics Processing Unit power by measuring
  WebGL and Canvas performance
e) Javascript group: executes number crunching by doing selected Array and
  String operations
Additionally Browsermark will test your browsers conformance, but conformance
tests are not included in this suite.
"""

import os

from telemetry import benchmark
from telemetry.page import page_measurement
from telemetry.page import page_set
from telemetry.value import scalar

class _BrowsermarkMeasurement(page_measurement.PageMeasurement):

  def MeasurePage(self, _, tab, results):
    # Select nearest server(North America=1) and start test.
    js_start_test =  """
        for (var i=0; i < $('#continent a').length; i++) {
          if (($('#continent a')[i]).getAttribute('data-id') == '1') {
            $('#continent a')[i].click();
            $('.start_test.enabled').click();
          }
        }
        """
    tab.ExecuteJavaScript(js_start_test)
    tab.WaitForJavaScriptExpression(
      'window.location.pathname.indexOf("results") != -1', 600)
    result = int(tab.EvaluateJavaScript(
        'document.getElementsByClassName("score")[0].innerHTML'))
    results.AddValue(
        scalar.ScalarValue(results.current_page, 'Score', 'score', result))


@benchmark.Disabled
class Browsermark(benchmark.Benchmark):
  """Browsermark suite tests CSS, DOM, resize, page load, WebGL and JS."""
  test = _BrowsermarkMeasurement
  def CreatePageSet(self, options):
    ps = page_set.PageSet(
      file_path=os.path.abspath(__file__),
      archive_data_file='../page_sets/data/browsermark.json',
      make_javascript_deterministic=False)
    ps.AddPageWithDefaultRunNavigate('http://browsermark.rightware.com/tests/')
    return ps
