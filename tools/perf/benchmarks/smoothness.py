# Copyright (c) 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import sys

from telemetry import test

from measurements import smoothness


class SmoothnessTop25(test.Test):
  """Measures rendering statistics while scrolling down the top 25 web pages.

  http://www.chromium.org/developers/design-documents/rendering-benchmarks"""
  test = smoothness.Smoothness
  page_set = 'page_sets/top_25.json'


class SmoothnessToughCanvasCases(test.Test):
  test = smoothness.Smoothness
  enabled = sys.platform != 'darwin'
  page_set = 'page_sets/tough_canvas_cases.json'


class SmoothnessKeyMobileSites(test.Test):
  """Measures rendering statistics while scrolling down the key mobile sites.

  http://www.chromium.org/developers/design-documents/rendering-benchmarks"""
  test = smoothness.Smoothness
  page_set = 'page_sets/key_mobile_sites.json'


class SmoothnessToughSchedulingCases(test.Test):
  """Measures rendering statistics while interacting with pages that have
  challenging scheduling properties.

  https://docs.google.com/a/chromium.org/document/d/
      17yhE5Po9By0sCdM1yZT3LiUECaUr_94rQt9j-4tOQIM/view"""
  test = smoothness.Smoothness
  page_set = 'page_sets/tough_scheduling_cases.json'
