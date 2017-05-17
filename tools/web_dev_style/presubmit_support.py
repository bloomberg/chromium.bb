# Copyright 2017 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.


import css_checker
import html_checker
import js_checker
import resource_checker


def CheckStyle(input_api, output_api):
  apis = input_api, output_api
  is_resource = lambda f: f.LocalPath().endswith(('.html', '.css', '.js'))
  checkers = [
      css_checker.CSSChecker(*apis, file_filter=is_resource),
      html_checker.HtmlChecker(*apis, file_filter=is_resource),
      js_checker.JSChecker(*apis, file_filter=is_resource),
      resource_checker.ResourceChecker(*apis, file_filter=is_resource),
  ]
  results = []
  for checker in checkers:
    results.extend(checker.RunChecks())
  return results
