# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from telemetry import test

from benchmarks import silk_flags
from measurements import rasterize_and_record


# RasterizeAndRecord disabled on linux because no raster times are reported and
# on mac because Chrome DCHECKS.
# TODO: Re-enable when unittests are happy on linux and mac: crbug.com/350684.

@test.Disabled('linux', 'mac')
class RasterizeAndRecordTop25(test.Test):
  """Measures rasterize and record performance on the top 25 web pages.

  http://www.chromium.org/developers/design-documents/rendering-benchmarks"""
  test = rasterize_and_record.RasterizeAndRecord
  page_set = 'page_sets/top_25.json'


@test.Disabled('linux', 'mac')
class RasterizeAndRecordKeyMobileSites(test.Test):
  """Measures rasterize and record performance on the key mobile sites.

  http://www.chromium.org/developers/design-documents/rendering-benchmarks"""
  test = rasterize_and_record.RasterizeAndRecord
  page_set = 'page_sets/key_mobile_sites.py'


@test.Disabled('linux', 'mac')
class RasterizeAndRecordSilk(test.Test):
  """Measures rasterize and record performance on the silk sites.

  http://www.chromium.org/developers/design-documents/rendering-benchmarks"""
  test = rasterize_and_record.RasterizeAndRecord
  page_set = 'page_sets/key_silk_cases.py'


@test.Disabled('linux', 'mac')
class RasterizeAndRecordFastPathSilk(test.Test):
  """Measures rasterize and record performance on the silk sites.

  Uses bleeding edge rendering fast paths.

  http://www.chromium.org/developers/design-documents/rendering-benchmarks"""
  tag = 'fast_path'
  test = rasterize_and_record.RasterizeAndRecord
  page_set = 'page_sets/key_silk_cases.py'
  def CustomizeBrowserOptions(self, options):
    silk_flags.CustomizeBrowserOptionsForFastPath(options)
