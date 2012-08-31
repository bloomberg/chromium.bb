# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
import os

import chrome_remote_control
import scroll_results

def Scroll(tab):
  scroll_js_path = os.path.join(os.path.dirname(__file__), 'scroll.js')
  scroll_js = open(scroll_js_path, 'r').read()

  # Run scroll test.
  tab.runtime.Evaluate(scroll_js)
  tab.runtime.Evaluate("""
    window.__scrollTestResult = null;
    new __ScrollTest(function(result) {
      window.__scrollTestResult = result;
    });
    null;  // Avoid returning the entire __ScrollTest object.
  """)

  # Poll for scroll result.
  chrome_remote_control.WaitFor(
      lambda: tab.runtime.Evaluate('window.__scrollTestResult'))

  # Get scroll results.
  url = tab.runtime.Evaluate('document.location.href')
  first_paint_secs = tab.runtime.Evaluate(
      'chrome.loadTimes().firstPaintTime - chrome.loadTimes().startLoadTime')
  results_list = tab.runtime.Evaluate('window.__scrollTestResult')

  return scroll_results.ScrollResults(url, first_paint_secs, results_list)
