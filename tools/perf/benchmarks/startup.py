# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
from telemetry import benchmark

from measurements import startup
import page_sets


@benchmark.Disabled('snowleopard') # crbug.com/336913
class StartupColdBlankPage(benchmark.Benchmark):
  tag = 'cold'
  test = startup.Startup
  page_set = page_sets.BlankPageSet
  options = {'cold': True,
             'pageset_repeat': 5}


class StartupWarmBlankPage(benchmark.Benchmark):
  tag = 'warm'
  test = startup.Startup
  page_set = page_sets.BlankPageSet
  options = {'warm': True,
             'pageset_repeat': 20}

@benchmark.Disabled('snowleopard') # crbug.com/336913
class StartupColdTheme(benchmark.Benchmark):
  tag = 'theme_cold'
  test = startup.Startup
  page_set = page_sets.BlankPageSet
  generated_profile_archive = 'theme_profile.zip'
  options = {'cold': True,
             'pageset_repeat': 5}


class StartupWarmTheme(benchmark.Benchmark):
  tag = 'theme_warm'
  test = startup.Startup
  page_set = page_sets.BlankPageSet
  generated_profile_archive = 'theme_profile.zip'
  options = {'warm': True,
             'pageset_repeat': 20}

@benchmark.Disabled('snowleopard') # crbug.com/336913
class StartupColdManyExtensions(benchmark.Benchmark):
  tag = 'many_extensions_cold'
  test = startup.Startup
  page_set = page_sets.BlankPageSet
  generated_profile_archive = 'many_extensions_profile.zip'
  options = {'cold': True,
             'pageset_repeat': 5}


class StartupWarmManyExtensions(benchmark.Benchmark):
  tag = 'many_extensions_warm'
  test = startup.Startup
  page_set = page_sets.BlankPageSet
  generated_profile_archive = 'many_extensions_profile.zip'
  options = {'warm': True,
             'pageset_repeat': 20}
