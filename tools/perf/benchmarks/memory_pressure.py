# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from telemetry import test
from measurements import memory_pressure
import page_sets


@test.Enabled('has tabs')
class MemoryPressure(test.Test):
  test = memory_pressure.MemoryPressure
  page_set = page_sets.Typical25PageSet
  options = {'pageset_repeat': 6}
