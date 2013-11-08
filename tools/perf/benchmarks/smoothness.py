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
  # TODO: Enable when crbug.com/314380 is fixed.
  enabled = sys.platform != 'linux2'
  test = smoothness.Smoothness
  page_set = 'page_sets/tough_canvas_cases.json'


class SmoothnessKeyMobileSites(test.Test):
  """Measures rendering statistics while scrolling down the key mobile sites.

  http://www.chromium.org/developers/design-documents/rendering-benchmarks"""
  # TODO(ernstm): remove when crbug.com/313753 is fixed.
  enabled = sys.platform != 'linux2'
  test = smoothness.Smoothness
  page_set = 'page_sets/key_mobile_sites.json'
