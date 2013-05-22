# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Runs Microsoft's RoboHornet Pro benchmark."""

import os

from telemetry.core import util
from telemetry.page import page_measurement
from telemetry.page import page_set

class RobohornetPro(page_measurement.PageMeasurement):
  def CreatePageSet(self, _, options):
    return page_set.PageSet.FromDict({
        'archive_data_file': '../data/robohornetpro.json',
        'pages': [
          { 'url':
            'http://ie.microsoft.com/testdrive/performance/robohornetpro/' }
          ]
        }, os.path.abspath(__file__))

  def CustomizeBrowserOptions(self, options):
    # Measurement require use of real Date.now() for measurement.
    options.wpr_make_javascript_deterministic = False

  def MeasurePage(self, _, tab, results):
    tab.ExecuteJavaScript('ToggleRoboHornet()')

    done = 'document.getElementById("results").innerHTML.indexOf("Total") != -1'
    def _IsDone():
      return tab.EvaluateJavaScript(done)
    util.WaitFor(_IsDone, 60)

    result = int(tab.EvaluateJavaScript('stopTime - startTime'))
    results.Add('Total', 'ms', result)
