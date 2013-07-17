# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os

from telemetry import test
from telemetry.core import util
from telemetry.page import page_measurement
from telemetry.page import page_set


class PicaMeasurement(page_measurement.PageMeasurement):
  def WillNavigateToPage(self, page, tab):
    page.script_to_evaluate_on_commit = """
      document.addEventListener('WebComponentsReady', function() {
        var unused = document.body.offsetHeight; // force layout
        window.__pica_load_time = performance.now();
      });
    """

  def MeasurePage(self, _, tab, results):
    def _IsDone():
      return tab.EvaluateJavaScript('window.__pica_load_time != undefined')
    util.WaitFor(_IsDone, 60)

    result = int(tab.EvaluateJavaScript('__pica_load_time'))
    results.Add('Total', 'ms', result)


class Pica(test.Test):
  test = PicaMeasurement

  def CreatePageSet(self, options):
    return page_set.PageSet.FromDict({
        'archive_data_file': '../data/pica.json',
        # Pica requires non-deterministic Date and Math.random calls
        # See http://crbug.com/255641
        'make_javascript_deterministic': False,
        'pages': [
          {
            'url': 'http://www.polymer-project.org/'
                   'polymer-all/projects/pica/index.html'
          }
        ]
      }, os.path.abspath(__file__))
