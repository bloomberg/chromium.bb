# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from measurements import power
import page_sets
from telemetry import benchmark


@benchmark.Enabled('android')
class PowerAndroidAcceptance(benchmark.Benchmark):
  """Android power acceptance test."""
  test = power.Power
  page_set = page_sets.AndroidAcceptancePageSet
  @classmethod
  def Name(cls):
    return 'power.android_acceptance'



@benchmark.Enabled('android')
class PowerTypical10Mobile(benchmark.Benchmark):
  """Android typical 10 mobile power test."""
  test = power.Power
  page_set = page_sets.Typical10MobilePageSet
  @classmethod
  def Name(cls):
    return 'power.typical_10_mobile'


