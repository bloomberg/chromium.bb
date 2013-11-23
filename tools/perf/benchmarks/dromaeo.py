# Copyright (c) 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os

from telemetry import test
from telemetry.core import util
from telemetry.page import page_measurement
from telemetry.page import page_set


class _DromaeoMeasurement(page_measurement.PageMeasurement):
  def MeasurePage(self, page, tab, results):
    tab.WaitForJavaScriptExpression(
        'window.document.cookie.indexOf("__done=1") >= 0', 600)

    js_get_results = 'JSON.stringify(window.automation.GetResults())'
    print js_get_results
    score = eval(tab.EvaluateJavaScript(js_get_results))

    def Escape(k):
      chars = [' ', '-', '/', '(', ')', '*']
      for c in chars:
        k = k.replace(c, '_')
      return k

    suffix = page.url[page.url.index('?') + 1 : page.url.index('&')]
    for k, v in score.iteritems():
      data_type = 'unimportant'
      if k == suffix:
        data_type = 'default'
      results.Add(Escape(k), 'runs/s', float(v), data_type=data_type)


class _DromaeoBenchmark(test.Test):
  """A base class for Dromaeo benchmarks."""
  test = _DromaeoMeasurement

  def CreatePageSet(self, options):
    """Makes a PageSet for Dromaeo benchmarks."""
    # Subclasses are expected to define a class member called query_param.
    if not hasattr(self, 'query_param'):
      raise NotImplementedError('query_param not in Dromaeo benchmark.')
    url = 'file://index.html?%s&automated' % self.query_param
    # The docstring of benchmark classes may also be used as a description
    # when 'run_benchmarks list' is run.
    description = self.__doc__ or 'Dromaeo JavaScript Benchmark'
    page_set_dict = {
        'description': description,
        'pages': [{'url': url}],
    }
    dromaeo_dir = os.path.join(util.GetChromiumSrcDir(),
                               'chrome', 'test', 'data', 'dromaeo')
    return page_set.PageSet.FromDict(page_set_dict, dromaeo_dir)


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

