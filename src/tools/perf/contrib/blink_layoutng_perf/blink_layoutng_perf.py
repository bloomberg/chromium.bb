# Copyright 2018 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
from benchmarks import blink_perf
from telemetry import benchmark

# pylint: disable=protected-access
@benchmark.Info(emails=['cbiesinger@chromium.org'],
                documentation_url='https://bit.ly/blink-perf-benchmarks')
class BlinkPerfLayoutNg(blink_perf._BlinkPerfBenchmark):
  subdir = 'layout'

  def SetExtraBrowserOptions(self, options):
    super(BlinkPerfLayoutNg, self).SetExtraBrowserOptions(options)
    options.AppendExtraBrowserArgs('--enable-blink-features=LayoutNG')

  @classmethod
  def Name(cls):
    return 'blink_perf.layout_ng'
