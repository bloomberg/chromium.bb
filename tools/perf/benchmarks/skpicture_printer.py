# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from measurements import skpicture_printer
from telemetry import benchmark
from telemetry.core import discover
from telemetry.page import page_set


def _MatchPageSetName(page_set_name, page_set_base_dir):
  page_sets = []
  page_sets += discover.DiscoverClasses(page_set_base_dir, page_set_base_dir,
                                        page_set.PageSet,
                                        index_by_class_name=True).values()
  for p in page_sets:
    if page_set_name == p.Name():
      return p
  return None


@benchmark.Disabled
class SkpicturePrinter(benchmark.Benchmark):
  @classmethod
  def AddBenchmarkCommandLineArgs(cls, parser):
    parser.add_option('--page-set-name',  action='store', type='string')
    parser.add_option('--page-set-base-dir', action='store', type='string')
    parser.add_option('-s', '--skp-outdir',
                      help='Output directory for the SKP files')
  @classmethod
  def ProcessCommandLineArgs(cls, parser, args):
    if not args.page_set_name:
      parser.error('Please specify --page-set-name')
    if not args.page_set_base_dir:
      parser.error('Please specify --page-set-base-dir')
    if not args.skp_outdir:
      parser.error('Please specify --skp-outdir')

  @classmethod
  def Name(cls):
    return 'skpicture_printer'

  def CreatePageTest(self, options):
    return skpicture_printer.SkpicturePrinter(options.skp_outdir)

  def CreatePageSet(self, options):
    page_set_class = _MatchPageSetName(options.page_set_name,
                                       options.page_set_base_dir)
    return page_set_class()
