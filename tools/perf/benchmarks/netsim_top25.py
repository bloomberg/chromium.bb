# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
from telemetry import test

from perf_tools import page_cycler


class NetsimTop25(test.Test):
  """Measures load time of the top 25 sites under simulated cable network."""
  test = page_cycler.PageCycler
  page_set = 'tools/perf/page_sets/top_25.json'
  options = {
    'extra_wpr_args': [
      '--shaping_type=proxy',
      '--net=cable'
      ],
    'pageset_repeat': '5',
    }

  def __init__(self):
    super(NetsimTop25, self).__init__()
    self.test.clear_cache_before_each_run = True
