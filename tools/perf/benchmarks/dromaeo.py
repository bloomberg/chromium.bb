# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import math
import os

from metrics import power
from telemetry import benchmark
from telemetry.page import page_set
from telemetry.page import page_test
from telemetry.value import scalar


class _DromaeoMeasurement(page_test.PageTest):
  def __init__(self):
    super(_DromaeoMeasurement, self).__init__()
    self._power_metric = None

  def CustomizeBrowserOptions(self, options):
    power.PowerMetric.CustomizeBrowserOptions(options)

  def WillStartBrowser(self, browser):
    self._power_metric = power.PowerMetric(browser)

  def DidNavigateToPage(self, page, tab):
    self._power_metric.Start(page, tab)

  def ValidateAndMeasurePage(self, page, tab, results):
    tab.WaitForJavaScriptExpression(
        'window.document.getElementById("pause") &&' +
        'window.document.getElementById("pause").value == "Run"',
        120)

    # Start spying on POST request that will report benchmark results, and
    # intercept result data.
    tab.ExecuteJavaScript('(function() {' +
                          '  var real_jquery_ajax_ = window.jQuery;' +
                          '  window.results_ = "";' +
                          '  window.jQuery.ajax = function(request) {' +
                          '    if (request.url == "store.php") {' +
                          '      window.results_ =' +
                          '          decodeURIComponent(request.data);' +
                          '      window.results_ = window.results_.substring(' +
                          '          window.results_.indexOf("=") + 1, ' +
                          '          window.results_.lastIndexOf("&"));' +
                          '      real_jquery_ajax_(request);' +
                          '    }' +
                          '  };' +
                          '})();')
    # Starts benchmark.
    tab.ExecuteJavaScript('window.document.getElementById("pause").click();')

    tab.WaitForJavaScriptExpression('!!window.results_', 600)

    self._power_metric.Stop(page, tab)
    self._power_metric.AddResults(tab, results)

    score = eval(tab.EvaluateJavaScript('window.results_ || "[]"'))

    def Escape(k):
      chars = [' ', '.', '-', '/', '(', ')', '*']
      for c in chars:
        k = k.replace(c, '_')
      return k

    def AggregateData(container, key, value):
      if key not in container:
        container[key] = {'count': 0, 'sum': 0}
      container[key]['count'] += 1
      container[key]['sum'] += math.log(value)

    suffix = page.url[page.url.index('?') + 1 :]
    def AddResult(name, value):
      important = False
      if name == suffix:
        important = True
      results.AddValue(scalar.ScalarValue(
          results.current_page, Escape(name), 'runs/s', value, important))

    aggregated = {}
    for data in score:
      AddResult('%s/%s' % (data['collection'], data['name']),
                data['mean'])

      top_name = data['collection'].split('-', 1)[0]
      AggregateData(aggregated, top_name, data['mean'])

      collection_name = data['collection']
      AggregateData(aggregated, collection_name, data['mean'])

    for key, value in aggregated.iteritems():
      AddResult(key, math.exp(value['sum'] / value['count']))

class _DromaeoBenchmark(benchmark.Benchmark):
  """A base class for Dromaeo benchmarks."""
  test = _DromaeoMeasurement

  def CreatePageSet(self, options):
    """Makes a PageSet for Dromaeo benchmarks."""
    # Subclasses are expected to define class members called query_param and
    # tag.
    if not hasattr(self, 'query_param') or not hasattr(self, 'tag'):
      raise NotImplementedError('query_param or tag not in Dromaeo benchmark.')
    archive_data_file = '../page_sets/data/dromaeo.%s.json' % self.tag
    ps = page_set.PageSet(
        make_javascript_deterministic=False,
        archive_data_file=archive_data_file,
        file_path=os.path.abspath(__file__))
    url = 'http://dromaeo.com?%s' % self.query_param
    ps.AddPageWithDefaultRunNavigate(url)
    return ps


class DromaeoDomCoreAttr(_DromaeoBenchmark):
  """Dromaeo DOMCore attr JavaScript benchmark."""
  tag = 'domcoreattr'
  query_param = 'dom-attr'


class DromaeoDomCoreModify(_DromaeoBenchmark):
  """Dromaeo DOMCore modify JavaScript benchmark."""
  tag = 'domcoremodify'
  query_param = 'dom-modify'


class DromaeoDomCoreQuery(_DromaeoBenchmark):
  """Dromaeo DOMCore query JavaScript benchmark."""
  tag = 'domcorequery'
  query_param = 'dom-query'


class DromaeoDomCoreTraverse(_DromaeoBenchmark):
  """Dromaeo DOMCore traverse JavaScript benchmark."""
  tag = 'domcoretraverse'
  query_param = 'dom-traverse'


class DromaeoJslibAttrJquery(_DromaeoBenchmark):
  """Dromaeo JSLib attr jquery JavaScript benchmark"""
  tag = 'jslibattrjquery'
  query_param = 'jslib-attr-jquery'


class DromaeoJslibAttrPrototype(_DromaeoBenchmark):
  """Dromaeo JSLib attr prototype JavaScript benchmark"""
  tag = 'jslibattrprototype'
  query_param = 'jslib-attr-prototype'


class DromaeoJslibEventJquery(_DromaeoBenchmark):
  """Dromaeo JSLib event jquery JavaScript benchmark"""
  tag = 'jslibeventjquery'
  query_param = 'jslib-event-jquery'


class DromaeoJslibEventPrototype(_DromaeoBenchmark):
  """Dromaeo JSLib event prototype JavaScript benchmark"""
  tag = 'jslibeventprototype'
  query_param = 'jslib-event-prototype'


@benchmark.Disabled('xp')  # crbug.com/389731
class DromaeoJslibModifyJquery(_DromaeoBenchmark):
  """Dromaeo JSLib modify jquery JavaScript benchmark"""
  tag = 'jslibmodifyjquery'
  query_param = 'jslib-modify-jquery'


class DromaeoJslibModifyPrototype(_DromaeoBenchmark):
  """Dromaeo JSLib modify prototype JavaScript benchmark"""
  tag = 'jslibmodifyprototype'
  query_param = 'jslib-modify-prototype'


class DromaeoJslibStyleJquery(_DromaeoBenchmark):
  """Dromaeo JSLib style jquery JavaScript benchmark"""
  tag = 'jslibstylejquery'
  query_param = 'jslib-style-jquery'


class DromaeoJslibStylePrototype(_DromaeoBenchmark):
  """Dromaeo JSLib style prototype JavaScript benchmark"""
  tag = 'jslibstyleprototype'
  query_param = 'jslib-style-prototype'


class DromaeoJslibTraverseJquery(_DromaeoBenchmark):
  """Dromaeo JSLib traverse jquery JavaScript benchmark"""
  tag = 'jslibtraversejquery'
  query_param = 'jslib-traverse-jquery'


class DromaeoJslibTraversePrototype(_DromaeoBenchmark):
  """Dromaeo JSLib traverse prototype JavaScript benchmark"""
  tag = 'jslibtraverseprototype'
  query_param = 'jslib-traverse-prototype'


class DromaeoCSSQueryJquery(_DromaeoBenchmark):
  """Dromaeo CSS Query jquery JavaScript benchmark"""
  tag = 'cssqueryjquery'
  query_param = 'cssquery-jquery'

