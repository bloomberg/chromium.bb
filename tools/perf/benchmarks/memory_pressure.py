# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from measurements import memory_pressure
from telemetry import test


@test.Disabled('android')
class MemoryPressure(test.Test):
  test = memory_pressure.MemoryPressure
  page_set = 'page_sets/typical_25.py'
  options = {'pageset_repeat': 6}
