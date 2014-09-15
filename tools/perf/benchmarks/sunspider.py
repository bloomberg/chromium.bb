# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
import collections
import json
import os

from metrics import power
from telemetry import benchmark
from telemetry.page import page_set
from telemetry.page import page_test
from telemetry.value import list_of_scalar_values


_URL = 'http://www.webkit.org/perf/sunspider-1.0.2/sunspider-1.0.2/driver.html'

DESCRIPTIONS = {
    '3d-cube':
        'Pure JavaScript computations of the kind you might use to do 3d '
        'rendering, but without the rendering. This ends up mostly hitting '
        'floating point math and array access.',
    '3d-morph':
        'Pure JavaScript computations of the kind you might use to do 3d '
        'rendering, but without the rendering. This ends up mostly hitting '
        'floating point math and array access.',
    '3d-raytrace':
        'Pure JavaScript computations of the kind you might use to do 3d '
        'rendering, but without the rendering. This ends up mostly hitting '
        'floating point math and array access.',
    'access-binary-trees': 'Array, object property and variable access.',
    'access-fannkuch': 'Array, object property and variable access.',
    'access-nbody': 'Array, object property and variable access.',
    'access-nsieve': 'Array, object property and variable access.',
    'bitops-3bit-bits-in-byte':
        'Bitwise operations, these can be useful for various things '
        'including games, mathematical computations, and various kinds of '
        'encoding/decoding. It\'s also the only kind of math in JavaScript '
        'that is done as integer, not floating point.',
    'bitops-bits-in-byte':
        'Bitwise operations, these can be useful for various things '
        'including games, mathematical computations, and various kinds of '
        'encoding/decoding. It\'s also the only kind of math in JavaScript '
        'that is done as integer, not floating point.',
    'bitops-bitwise-and':
        'Bitwise operations, these can be useful for various things '
        'including games, mathematical computations, and various kinds of '
        'encoding/decoding. It\'s also the only kind of math in JavaScript '
        'that is done as integer, not floating point.',
    'bitops-nsieve-bits':
        'Bitwise operations, these can be useful for various things '
        'including games, mathematical computations, and various kinds of '
        'encoding/decoding. It\'s also the only kind of math in JavaScript '
        'that is done as integer, not floating point.',
    'controlflow-recursive':
        'Control flow constructs (looping, recursion, conditionals). Right '
        'now it mostly covers recursion, as the others are pretty well covered '
        'by other tests.',
    'crypto-aes': 'Real cryptography code related to AES.',
    'crypto-md5': 'Real cryptography code related to MD5.',
    'crypto-sha1': 'Real cryptography code related to SHA1.',
    'date-format-tofte': 'Performance of JavaScript\'s "date" objects.',
    'date-format-xparb': 'Performance of JavaScript\'s "date" objects.',
    'math-cordic': 'Various mathematical type computations.',
    'math-partial-sums': 'Various mathematical type computations.',
    'math-spectral-norm': 'Various mathematical type computations.',
    'regexp-dna': 'Regular expressions performance.',
    'string-base64': 'String processing.',
    'string-fasta': 'String processing',
    'string-tagcloud': 'String processing code to generate a giant "tagcloud".',
    'string-unpack-code': 'String processing code to extracting compressed JS.',
    'string-validate-input': 'String processing.',
}


class _SunspiderMeasurement(page_test.PageTest):
  def __init__(self):
    super(_SunspiderMeasurement, self).__init__()
    self._power_metric = None

  def CustomizeBrowserOptions(self, options):
    power.PowerMetric.CustomizeBrowserOptions(options)

  def WillStartBrowser(self, browser):
    self._power_metric = power.PowerMetric(browser)

  def DidNavigateToPage(self, page, tab):
    self._power_metric.Start(page, tab)

  def ValidateAndMeasurePage(self, page, tab, results):
    tab.WaitForJavaScriptExpression(
        'window.location.pathname.indexOf("results.html") >= 0'
        '&& typeof(output) != "undefined"', 300)

    self._power_metric.Stop(page, tab)
    self._power_metric.AddResults(tab, results)

    js_get_results = 'JSON.stringify(output);'
    js_results = json.loads(tab.EvaluateJavaScript(js_get_results))

    # Below, r is a map of benchmark names to lists of result numbers,
    # and totals is a list of totals of result numbers.
    # js_results is: formatted like this:
    # [
    #   {'3d-cube': v1, '3d-morph': v2, ...},
    #   {'3d-cube': v3, '3d-morph': v4, ...},
    #   ...
    # ]
    r = collections.defaultdict(list)
    totals = []
    for result in js_results:
      total = 0
      for key, value in result.iteritems():
        r[key].append(value)
        total += value
      totals.append(total)
    for key, values in r.iteritems():
      results.AddValue(list_of_scalar_values.ListOfScalarValues(
          results.current_page, key, 'ms', values, important=False,
          description=DESCRIPTIONS.get(key)))
    results.AddValue(list_of_scalar_values.ListOfScalarValues(
        results.current_page, 'Total', 'ms', totals,
        description='Totals of run time for each different type of benchmark '
                    'in sunspider'))


class Sunspider(benchmark.Benchmark):
  """Apple's SunSpider JavaScript benchmark."""
  test = _SunspiderMeasurement

  def CreatePageSet(self, options):
    ps = page_set.PageSet(
      archive_data_file='../page_sets/data/sunspider.json',
      make_javascript_deterministic=False,
      file_path=os.path.abspath(__file__))
    ps.AddPageWithDefaultRunNavigate(_URL)
    return ps
