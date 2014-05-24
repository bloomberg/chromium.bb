# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from benchmarks import silk_flags
from measurements import rasterize_and_record_micro
from telemetry import test


# RasterizeAndRecord disabled on mac because of crbug.com/350684.
# RasterizeAndRecord disabled on windows because of crbug.com/338057.
@test.Disabled('mac', 'win')
class RasterizeAndRecordMicroTop25(test.Test):
  """Measures rasterize and record performance on the top 25 web pages.

  http://www.chromium.org/developers/design-documents/rendering-benchmarks"""
  test = rasterize_and_record_micro.RasterizeAndRecordMicro
  page_set = 'page_sets/top_25.py'


@test.Disabled('mac', 'win')
class RasterizeAndRecordMicroKeyMobileSites(test.Test):
  """Measures rasterize and record performance on the key mobile sites.

  http://www.chromium.org/developers/design-documents/rendering-benchmarks"""
  test = rasterize_and_record_micro.RasterizeAndRecordMicro
  page_set = 'page_sets/key_mobile_sites.py'


@test.Disabled('mac', 'win')
class RasterizeAndRecordMicroKeySilkCases(test.Test):
  """Measures rasterize and record performance on the silk sites.

  http://www.chromium.org/developers/design-documents/rendering-benchmarks"""
  test = rasterize_and_record_micro.RasterizeAndRecordMicro
  page_set = 'page_sets/key_silk_cases.py'


@test.Disabled('mac', 'win')
class RasterizeAndRecordMicroFastPathGpuRasterizationKeySilkCases(test.Test):
  """Measures rasterize and record performance on the silk sites.

  Uses GPU rasterization together with bleeding edge rendering fast paths.

  http://www.chromium.org/developers/design-documents/rendering-benchmarks"""
  tag = 'fast_path_gpu_rasterization'
  test = rasterize_and_record_micro.RasterizeAndRecordMicro
  page_set = 'page_sets/key_silk_cases.py'
  def CustomizeBrowserOptions(self, options):
    silk_flags.CustomizeBrowserOptionsForFastPath(options)
    silk_flags.CustomizeBrowserOptionsForGpuRasterization(options)


@test.Enabled('android')
class RasterizeAndRecordMicroPolymer(test.Test):
  """Measures rasterize and record performance on the Polymer cases.

  http://www.chromium.org/developers/design-documents/rendering-benchmarks"""
  test = rasterize_and_record_micro.RasterizeAndRecordMicro
  page_set = 'page_sets/polymer.py'
