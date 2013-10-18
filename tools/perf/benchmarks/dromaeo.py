# Copyright (c) 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os

from telemetry import test
from telemetry.core import util
from telemetry.page import page_set

from measurements import dromaeo


class DromaeoBenchmark(test.Test):
  """A base class for Dromaeo benchmarks."""
  test = dromaeo.Dromaeo

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


class DromaeoDomCoreAttr(DromaeoBenchmark):
  """Dromaeo DOMCore attr JavaScript benchmark."""
  tag = 'domcoreattr'
  query_param = 'dom-attr'


class DromaeoDomCoreModify(DromaeoBenchmark):
  """Dromaeo DOMCore modify JavaScript benchmark."""
  tag = 'domcoremodify'
  query_param = 'dom-modify'


class DromaeoDomCoreQuery(DromaeoBenchmark):
  """Dromaeo DOMCore query JavaScript benchmark."""
  tag = 'domcorequery'
  query_param = 'dom-query'


class DromaeoDomCoreTraverse(DromaeoBenchmark):
  """Dromaeo DOMCore traverse JavaScript benchmark."""
  tag = 'domcoretraverse'
  query_param = 'dom-traverse'


class DromaeoJslibAttrJquery(DromaeoBenchmark):
  """Dromaeo JSLib attr jquery JavaScript benchmark"""
  tag = 'jslibattrjquery'
  query_param = 'jslib-attr-jquery'


class DromaeoJslibAttrPrototype(DromaeoBenchmark):
  """Dromaeo JSLib attr prototype JavaScript benchmark"""
  tag = 'jslibattrprototype'
  query_param = 'jslib-attr-prototype'


class DromaeoJslibEventJquery(DromaeoBenchmark):
  """Dromaeo JSLib event jquery JavaScript benchmark"""
  tag = 'jslibeventjquery'
  query_param = 'jslib-event-jquery'


class DromaeoJslibEventPrototype(DromaeoBenchmark):
  """Dromaeo JSLib event prototype JavaScript benchmark"""
  tag = 'jslibeventprototype'
  query_param = 'jslib-event-prototype'


class DromaeoJslibModifyJquery(DromaeoBenchmark):
  """Dromaeo JSLib modify jquery JavaScript benchmark"""
  tag = 'jslibmodifyjquery'
  query_param = 'jslib-modify-jquery'


class DromaeoJslibModifyPrototype(DromaeoBenchmark):
  """Dromaeo JSLib modify prototype JavaScript benchmark"""
  tag = 'jslibmodifyprototype'
  query_param = 'jslib-modify-prototype'


class DromaeoJslibStyleJquery(DromaeoBenchmark):
  """Dromaeo JSLib style jquery JavaScript benchmark"""
  tag = 'jslibstylejquery'
  query_param = 'jslib-style-jquery'


class DromaeoJslibStylePrototype(DromaeoBenchmark):
  """Dromaeo JSLib style prototype JavaScript benchmark"""
  tag = 'jslibstyleprototype'
  query_param = 'jslib-style-prototype'


class DromaeoJslibTraverseJquery(DromaeoBenchmark):
  """Dromaeo JSLib traverse jquery JavaScript benchmark"""
  tag = 'jslibtraversejquery'
  query_param = 'jslib-traverse-jquery'


class DromaeoJslibTraversePrototype(DromaeoBenchmark):
  """Dromaeo JSLib traverse prototype JavaScript benchmark"""
  tag = 'jslibtraverseprototype'
  query_param = 'jslib-traverse-prototype'

