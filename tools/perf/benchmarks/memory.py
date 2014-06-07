# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
from telemetry import test

from measurements import memory
import page_sets


@test.Disabled('android')  # crbug.com/370977
class MemoryMobile(test.Test):
  test = memory.Memory
  page_set = page_sets.MobileMemoryPageSet


class MemoryTop25(test.Test):
  test = memory.Memory
  page_set = page_sets.Top25PageSet


class Reload2012Q3(test.Test):
  tag = 'reload'
  test = memory.Memory
  page_set = page_sets.Top2012Q3PageSet


@test.Disabled('android')  # crbug.com/371153
class MemoryToughDomMemoryCases(test.Test):
  test = memory.Memory
  page_set = page_sets.ToughDomMemoryCasesPageSet
