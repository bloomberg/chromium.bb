# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from measurements import rasterize_and_record_micro
import page_sets
from telemetry import benchmark


class _RasterizeAndRecordMicro(benchmark.Benchmark):
  @classmethod
  def AddBenchmarkCommandLineArgs(cls, parser):
    parser.add_option('--start-wait-time', type='float',
                      default=2,
                      help='Wait time before the benchmark is started '
                      '(must be long enought to load all content)')
    parser.add_option('--rasterize-repeat', type='int',
                      default=100,
                      help='Repeat each raster this many times. Increase '
                      'this value to reduce variance.')
    parser.add_option('--record-repeat', type='int',
                      default=100,
                      help='Repeat each record this many times. Increase '
                      'this value to reduce variance.')
    parser.add_option('--timeout', type='int',
                      default=120,
                      help='The length of time to wait for the micro '
                      'benchmark to finish, expressed in seconds.')
    parser.add_option('--report-detailed-results',
                      action='store_true',
                      help='Whether to report additional detailed results.')

  @classmethod
  def Name(cls):
    return 'rasterize_and_record_micro'

  def CreatePageTest(self, options):
    return rasterize_and_record_micro.RasterizeAndRecordMicro(
        options.start_wait_time, options.rasterize_repeat,
        options.record_repeat, options.timeout, options.report_detailed_results)


# RasterizeAndRecord disabled on mac because of crbug.com/350684.
# RasterizeAndRecord disabled on windows because of crbug.com/338057.
@benchmark.Disabled('mac', 'win')
class RasterizeAndRecordMicroTop25(_RasterizeAndRecordMicro):
  """Measures rasterize and record performance on the top 25 web pages.

  http://www.chromium.org/developers/design-documents/rendering-benchmarks"""
  page_set = page_sets.Top25PageSet

  @classmethod
  def Name(cls):
    return 'rasterize_and_record_micro.top_25_smooth'


# RasterizeAndRecord disabled on mac because of crbug.com/350684.
# RasterizeAndRecord disabled on windows because of crbug.com/338057.
@benchmark.Disabled('mac', 'win')
@benchmark.Disabled('reference')
class RasterizeAndRecordMicroTop25WithSlimmingPaint(_RasterizeAndRecordMicro):
  """Measures rasterize and record performance with --enable-slimming-paint.

  http://www.chromium.org/developers/design-documents/rendering-benchmarks"""
  page_set = page_sets.Top25PageSet

  def CustomizeBrowserOptions(self, options):
    options.AppendExtraBrowserArgs(['--enable-slimming-paint'])

  @classmethod
  def Name(cls):
    return 'rasterize_and_record_micro.top_25_slimming_paint_smooth'


@benchmark.Disabled('mac', 'win')
class RasterizeAndRecordMicroKeyMobileSites(_RasterizeAndRecordMicro):
  """Measures rasterize and record performance on the key mobile sites.

  http://www.chromium.org/developers/design-documents/rendering-benchmarks"""
  page_set = page_sets.KeyMobileSitesPageSet

  @classmethod
  def Name(cls):
    return 'rasterize_and_record_micro.key_mobile_sites_smooth'


@benchmark.Disabled('mac', 'win')
class RasterizeAndRecordMicroKeySilkCases(_RasterizeAndRecordMicro):
  """Measures rasterize and record performance on the silk sites.

  http://www.chromium.org/developers/design-documents/rendering-benchmarks"""

  @classmethod
  def Name(cls):
    return 'rasterize_and_record_micro.key_silk_cases'

  def CreatePageSet(self, options):
    return page_sets.KeySilkCasesPageSet(run_no_page_interactions=True)


@benchmark.Enabled('android')
class RasterizeAndRecordMicroPolymer(_RasterizeAndRecordMicro):
  """Measures rasterize and record performance on the Polymer cases.

  http://www.chromium.org/developers/design-documents/rendering-benchmarks"""

  @classmethod
  def Name(cls):
    return 'rasterize_and_record_micro.polymer'

  def CreatePageSet(self, options):
    return page_sets.PolymerPageSet(run_no_page_interactions=True)
