# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from telemetry.page import page_measurement

class Dromaeo(page_measurement.PageMeasurement):
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
      results.Add(Escape(k), 'runs/s', v, data_type=data_type)
