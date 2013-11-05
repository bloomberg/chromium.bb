# Copyright (c) 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os

from telemetry import test
from telemetry.core import util

from measurements import blink_perf


class BlinkPerfAll(test.Test):
  tag = 'all'
  test = blink_perf.BlinkPerfMeasurement

  def CreatePageSet(self, options):
    path = os.path.join(util.GetChromiumSrcDir(),
        'third_party', 'WebKit', 'PerformanceTests')
    return blink_perf.CreatePageSetFromPath(path)

class BlinkPerfAnimation(test.Test):
  tag = 'animation'
  test = blink_perf.BlinkPerfMeasurement

  def CreatePageSet(self, options):
    path = os.path.join(util.GetChromiumSrcDir(),
        'third_party', 'WebKit', 'PerformanceTests', 'Animation')
    return blink_perf.CreatePageSetFromPath(path)

class BlinkPerfWebAnimations(test.Test):
  tag = 'web_animations'
  test = blink_perf.BlinkPerfMeasurement

  def CreatePageSet(self, options):
    path = os.path.join(util.GetChromiumSrcDir(),
        'third_party', 'WebKit', 'PerformanceTests', 'Animation')
    return blink_perf.CreatePageSetFromPath(path)

  def CustomizeBrowserOptions(self, options):
    options.AppendExtraBrowserArgs('--enable-web-animations-css')
