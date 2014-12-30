# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from measurements import startup
import page_sets
from telemetry import benchmark


class _StartupCold(benchmark.Benchmark):
  """Measures cold startup time with a clean profile."""
  options = {'pageset_repeat': 5}

  def CreatePageTest(self, options):
    return startup.Startup(cold=True)


class _StartupWarm(benchmark.Benchmark):
  """Measures warm startup time with a clean profile."""
  options = {'pageset_repeat': 20}

  def CreatePageTest(self, options):
    return startup.Startup(cold=False)


@benchmark.Enabled('has tabs')
@benchmark.Disabled('snowleopard') # crbug.com/336913
class StartupColdBlankPage(_StartupCold):
  """Measures cold startup time with a clean profile."""
  tag = 'cold'
  page_set = page_sets.BlankPageSet


@benchmark.Enabled('has tabs')
class StartupWarmBlankPage(_StartupWarm):
  """Measures warm startup time with a clean profile."""
  tag = 'warm'
  page_set = page_sets.BlankPageSet


@benchmark.Disabled  # crbug.com/336913
class StartupColdTheme(_StartupCold):
  tag = 'theme_cold'
  page_set = page_sets.BlankPageSet
  generated_profile_archive = 'theme_profile.zip'


@benchmark.Disabled
class StartupWarmTheme(_StartupWarm):
  tag = 'theme_warm'
  page_set = page_sets.BlankPageSet
  generated_profile_archive = 'theme_profile.zip'


@benchmark.Disabled  # crbug.com/336913
class StartupColdManyExtensions(_StartupCold):
  tag = 'many_extensions_cold'
  page_set = page_sets.BlankPageSet
  generated_profile_archive = 'many_extensions_profile.zip'


@benchmark.Disabled
class StartupWarmManyExtensions(_StartupWarm):
  tag = 'many_extensions_warm'
  page_set = page_sets.BlankPageSet
  generated_profile_archive = 'many_extensions_profile.zip'
