# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from benchmarks import silk_flags
from measurements import rasterize_and_record_micro
from telemetry import test


# RasterizeAndRecord disabled on mac because Chrome DCHECKS.
# TODO: Re-enable when unittests are happy: crbug.com/350684.
# TODO(skyostil): Re-enable on windows (crbug.com/360666).

@test.Disabled
class RasterizeAndRecordMicroTop25(test.Test):
  """Measures rasterize and record performance on the top 25 web pages.

  http://www.chromium.org/developers/design-documents/rendering-benchmarks"""
  test = rasterize_and_record_micro.RasterizeAndRecordMicro
  page_set = 'page_sets/top_25.json'


@test.Disabled('mac')
class RasterizeAndRecordMicroKeyMobileSites(test.Test):
  """Measures rasterize and record performance on the key mobile sites.

  http://www.chromium.org/developers/design-documents/rendering-benchmarks"""
  test = rasterize_and_record_micro.RasterizeAndRecordMicro
  page_set = 'page_sets/key_mobile_sites.py'


@test.Disabled('mac')
class RasterizeAndRecordMicroKeySilkCases(test.Test):
  """Measures rasterize and record performance on the silk sites.

  http://www.chromium.org/developers/design-documents/rendering-benchmarks"""
  test = rasterize_and_record_micro.RasterizeAndRecordMicro
  page_set = 'page_sets/key_silk_cases.py'


@test.Disabled('mac')
class RasterizeAndRecordMicroFastPathKeySilkCases(test.Test):
  """Measures rasterize and record performance on the silk sites.

  Uses bleeding edge rendering fast paths.

  http://www.chromium.org/developers/design-documents/rendering-benchmarks"""
  tag = 'fast_path'
  test = rasterize_and_record_micro.RasterizeAndRecordMicro
  page_set = 'page_sets/key_silk_cases.py'
  def CustomizeBrowserOptions(self, options):
    silk_flags.CustomizeBrowserOptionsForFastPath(options)
