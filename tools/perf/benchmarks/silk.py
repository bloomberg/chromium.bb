# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
from telemetry import test

from measurements import thread_times

class SilkKeySilkCases(test.Test):
  """Measures timeline metrics while performing smoothness action on
  key silk cases.

  http://www.chromium.org/developers/design-documents/rendering-benchmarks"""
  test = thread_times.ThreadTimes
  page_set = 'page_sets/key_silk_cases.json'
  options = {"report_silk_results": True}
