# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from telemetry import test

from benchmarks import silk_flags
from measurements import rasterize_and_record


# RasterizeAndRecord disabled on linux because no raster times are reported.
# TODO: re-enable when unittests are happy on linux.

@test.Disabled('linux')
class RasterizeAndRecordTop25(test.Test):
  """Measures rasterize and record performance on the top 25 web pages.

  http://www.chromium.org/developers/design-documents/rendering-benchmarks"""
  test = rasterize_and_record.RasterizeAndRecord
  page_set = 'page_sets/top_25.json'


@test.Disabled('linux')
class RasterizeAndRecordKeyMobileSites(test.Test):
  """Measures rasterize and record performance on the key mobile sites.

  http://www.chromium.org/developers/design-documents/rendering-benchmarks"""
  test = rasterize_and_record.RasterizeAndRecord
  page_set = 'page_sets/key_mobile_sites.json'


@test.Disabled('linux')
class RasterizeAndRecordSilk(test.Test):
  """Measures rasterize and record performance on the silk sites.

  http://www.chromium.org/developers/design-documents/rendering-benchmarks"""
  test = rasterize_and_record.RasterizeAndRecord
  page_set = 'page_sets/key_silk_cases.json'


class RasterizeAndRecordFastPathSilk(test.Test):
  """Measures rasterize and record performance on the silk sites.

  Uses bleeding edge rendering fast paths.

  http://www.chromium.org/developers/design-documents/rendering-benchmarks"""
  tag = 'fast_path'
  test = rasterize_and_record.RasterizeAndRecord
  page_set = 'page_sets/key_silk_cases.json'
  def CustomizeBrowserOptions(self, options):
    silk_flags.CustomizeBrowserOptionsForFastPath(options)
