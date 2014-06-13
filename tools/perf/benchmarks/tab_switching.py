# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from telemetry import test

from measurements import tab_switching


@test.Disabled('android')  # crbug.com/379561
class TabSwitchingTop10(test.Test):
  test = tab_switching.TabSwitching
  page_set = 'page_sets/top_10.py'


@test.Disabled('android')  # crbug.com/379561
class TabSwitchingTypical25(test.Test):
  test = tab_switching.TabSwitching
  page_set = 'page_sets/typical_25.py'


@test.Disabled('android')  # crbug.com/379561
class TabSwitchingFiveBlankTabs(test.Test):
  test = tab_switching.TabSwitching
  page_set = 'page_sets/five_blank_pages.py'
  options = {'pageset_repeat': 10}


@test.Disabled('android')  # crbug.com/379561
class TabSwitchingToughEnergyCases(test.Test):
  test = tab_switching.TabSwitching
  page_set = 'page_sets/tough_energy_cases.py'
  options = {'pageset_repeat': 10}
