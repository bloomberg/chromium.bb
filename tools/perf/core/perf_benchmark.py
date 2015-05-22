# Copyright 2015 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os
import sys

from telemetry import benchmark
from telemetry.core import browser_finder

sys.path.append(os.path.join(os.path.dirname(__file__), os.pardir,
    os.pardir, 'variations'))
import fieldtrial_util # pylint: disable=import-error


class PerfBenchmark(benchmark.Benchmark):

  def SetExtraBrowserOptions(self, options):
    """ To be overridden by perf benchmarks. """
    pass

  def CustomizeBrowserOptions(self, options):
    # Subclass of PerfBenchmark should override  SetExtraBrowserOptions to add
    # more browser options rather than overriding CustomizeBrowserOptions.
    super(PerfBenchmark, self).CustomizeBrowserOptions(options)
    variations = self._GetVariationsBrowserArgs(options.finder_options)
    options.AppendExtraBrowserArgs(variations)
    self.SetExtraBrowserOptions(options)

  @staticmethod
  def _FixupTargetOS(target_os):
    if target_os == 'darwin':
      return 'mac'
    if target_os.startswith('win'):
      return 'win'
    if target_os.startswith('linux'):
      return 'linux'
    return target_os

  def _GetVariationsBrowserArgs(self, finder_options):
    variations_dir = os.path.join(os.path.dirname(__file__), os.pardir,
        os.pardir, os.pardir, 'testing', 'variations')
    target_os = browser_finder.FindBrowser(finder_options).target_os
    base_variations_path = os.path.join(variations_dir,
        'fieldtrial_testing_config.json')
    return fieldtrial_util.GenerateArgs(base_variations_path,
        os.path.join(variations_dir,
        'fieldtrial_testing_config_%s.json' % self._FixupTargetOS(target_os)))
