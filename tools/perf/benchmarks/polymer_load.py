# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from measurements import polymer_load
import page_sets
from telemetry import benchmark


class PolymerLoadPica(benchmark.Benchmark):
  """Measures time to polymer-ready for PICA."""
  test = polymer_load.PolymerLoadMeasurement
  page_set = page_sets.PicaPageSet
