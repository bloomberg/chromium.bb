# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from core import perf_benchmark

import ct_benchmarks_util
from measurements import smoothness

import page_sets
from page_sets import repaint_helpers

from telemetry import benchmark

# Disabled because we do not plan on running CT benchmarks on the perf
# waterfall any time soon.
@benchmark.Disabled('all')
class RepaintCT(perf_benchmark.PerfBenchmark):
  """Measures repaint performance for Cluster Telemetry."""

  @classmethod
  def Name(cls):
    return 'repaint_ct'

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
    ct_benchmarks_util.AddBenchmarkCommandLineArgs(parser)

  @classmethod
  def ProcessCommandLineArgs(cls, parser, args):
    ct_benchmarks_util.ValidateCommandLineArgs(parser, args)

  def CreateStorySet(self, options):
    return page_sets.CTPageSet(
        options.urls_list, options.user_agent, options.archive_data_file,
        run_page_interaction_callback=repaint_helpers.WaitThenRepaint)

  def CreatePageTest(self, options):
    return smoothness.Repaint()
