# Copyright (c) 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
from telemetry import test

from measurements import memory
from page_sets import mobile_memory
from page_sets import top_25
from page_sets import top_desktop_sites_2012Q3
from page_sets import tough_dom_memory_cases


@test.Disabled('android')  # crbug.com/370977
class MemoryMobile(test.Test):
  test = memory.Memory
  page_set = mobile_memory.MobileMemoryPageSet()


class MemoryTop25(test.Test):
  test = memory.Memory
  page_set = top_25.Top25PageSet()


class Reload2012Q3(test.Test):
  tag = 'reload'
  test = memory.Memory
  page_set = top_desktop_sites_2012Q3.Top2012Q3PageSet()


@test.Disabled('android')  # crbug.com/371153
class MemoryToughDomMemoryCases(test.Test):
  test = memory.Memory
  page_set = tough_dom_memory_cases.ToughDomMemoryCasesPageSet()
