# Copyright 2015 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from benchmarks import startup2
import page_sets

from telemetry import benchmark


# TODO(gabadie): Replaces start_with_url.* by start_with_url2.* after confirming
# that both benchmarks produce the same results.

# Disable accessing protected member for startup2._StartupPerfBenchmark. It
# needs to be protected to not be listed in the list of benchmarks to run, even
# though its purpose is only to factorise common code between startup
# benchmarks.
# pylint: disable=protected-access

@benchmark.Enabled('has tabs')
@benchmark.Enabled('android')
@benchmark.Disabled('chromeos', 'linux', 'mac', 'win')
class StartWithUrlColdTBM(startup2._StartupPerfBenchmark):
  """Measures time to start Chrome cold with startup URLs."""

  page_set = page_sets.StartupPagesPageSetTBM
  options = {'pageset_repeat': 5}

  def SetExtraBrowserOptions(self, options):
      options.clear_sytem_cache_for_browser_and_profile_on_start = True
      super(StartWithUrlColdTBM, self).SetExtraBrowserOptions(options)

  @classmethod
  def Name(cls):
    return 'start_with_url2.cold.startup_pages'


@benchmark.Enabled('has tabs')
@benchmark.Enabled('android')
@benchmark.Disabled('chromeos', 'linux', 'mac', 'win')
class StartWithUrlWarmTBM(startup2._StartupPerfBenchmark):
  """Measures stimetime to start Chrome warm with startup URLs."""

  page_set = page_sets.StartupPagesPageSetTBM
  options = {'pageset_repeat': 11}

  @classmethod
  def Name(cls):
    return 'start_with_url2.warm.startup_pages'

  @classmethod
  def ValueCanBeAddedPredicate(cls, value, is_first_result):
    del value  # unused
    # Ignores first results because the first invocation is actualy cold since
    # we are loading the profile for the first time.
    return not is_first_result
