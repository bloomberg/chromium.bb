# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
from telemetry import test

from measurements import smoothness


class SilkKeySilkCases(test.Test):
  """Measures timeline metrics while scrolling down the key_silk_cases pages.

  http://www.chromium.org/developers/design-documents/rendering-benchmarks"""
  test = smoothness.Smoothness
  page_set = 'page_sets/key_silk_cases.json'
  options = {'metric': 'timeline'}
