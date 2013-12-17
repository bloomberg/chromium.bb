# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
from telemetry import test

from measurements import smoothness

class FastPathCases(test.Test):
  """Measures timeline metrics while scrolling down fast-path mobile sites.
  http://www.chromium.org/developers/design-documents/rendering-benchmarks"""
  test = smoothness.Smoothness
  page_set = 'page_sets/key_mobile_sites.json'
  options = {'metric': 'timeline', 'page_label_filter' : 'fastpath'}
