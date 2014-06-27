# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
from telemetry import benchmark

from measurements import startup
import page_sets


@benchmark.Disabled('snowleopard') # crbug.com/336913
class StartWithUrlCold(benchmark.Benchmark):
  """Measure time to start Chrome cold with startup URLs"""
  tag = 'cold'
  test = startup.StartWithUrl
  page_set = page_sets.StartupPagesPageSet
  options = {'cold': True,
             'pageset_repeat': 5}

class StartWithUrlWarm(benchmark.Benchmark):
  """Measure time to start Chrome warm with startup URLs"""
  tag = 'warm'
  test = startup.StartWithUrl
  page_set = page_sets.StartupPagesPageSet
  options = {'warm': True,
             'pageset_repeat': 10}

