# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from core import perf_benchmark

from measurements import startup
import page_sets
from telemetry import benchmark


class _StartWithUrl(perf_benchmark.PerfBenchmark):
  page_set = page_sets.StartupPagesPageSet
  test = startup.StartWithUrl

  @classmethod
  def Name(cls):
    return 'start_with_url.startup_pages'

  def CreatePageTest(self, options):
    is_cold = (self.tag == 'cold')
    return self.test(cold=is_cold)


@benchmark.Enabled('has tabs')
@benchmark.Disabled('chromeos', 'linux', 'mac', 'win')
class StartWithUrlCold(_StartWithUrl):
  """Measure time to start Chrome cold with startup URLs"""
  tag = 'cold'
  options = {'pageset_repeat': 5}

  @classmethod
  def Name(cls):
    return 'start_with_url.cold.startup_pages'


@benchmark.Enabled('has tabs')
@benchmark.Disabled('chromeos', 'linux', 'mac', 'win')
class StartWithUrlWarm(_StartWithUrl):
  """Measure time to start Chrome warm with startup URLs"""
  tag = 'warm'
  options = {'pageset_repeat': 10}
  @classmethod
  def Name(cls):
    return 'start_with_url.warm.startup_pages'

