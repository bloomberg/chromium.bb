# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from benchmarks import silk_flags
from measurements import repaint as repaint_measurement
import page_sets
from telemetry import benchmark


class _Repaint(benchmark.Benchmark):
  @classmethod
  def AddBenchmarkCommandLineArgs(cls, parser):
    parser.add_option('--mode', type='string',
                      default='viewport',
                      help='Invalidation mode. '
                      'Supported values: fixed_size, layer, random, viewport.')
    parser.add_option('--width', type='int',
                      default=None,
                      help='Width of invalidations for fixed_size mode.')
    parser.add_option('--height', type='int',
                      default=None,
                      help='Height of invalidations for fixed_size mode.')

  @classmethod
  def Name(cls):
    return 'repaint'

  def CreatePageTest(self, options):
    return repaint_measurement.Repaint(options.mode, options.width,
                                       options.height)

@benchmark.Enabled('android')
class RepaintKeyMobileSites(_Repaint):
  """Measures repaint performance on the key mobile sites.

  http://www.chromium.org/developers/design-documents/rendering-benchmarks"""
  page_set = page_sets.KeyMobileSitesRepaintPageSet

  @classmethod
  def Name(cls):
    return 'repaint.key_mobile_sites_repaint'


@benchmark.Enabled('android')
class RepaintGpuRasterizationKeyMobileSites(_Repaint):
  """Measures repaint performance on the key mobile sites with forced GPU
  rasterization.

  http://www.chromium.org/developers/design-documents/rendering-benchmarks"""
  tag = 'gpu_rasterization'
  page_set = page_sets.KeyMobileSitesRepaintPageSet
  def CustomizeBrowserOptions(self, options):
    silk_flags.CustomizeBrowserOptionsForGpuRasterization(options)
  @classmethod
  def Name(cls):
    return 'repaint.gpu_rasterization.key_mobile_sites_repaint'

