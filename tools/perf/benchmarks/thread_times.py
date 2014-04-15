# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
from telemetry import test

from benchmarks import silk_flags
from measurements import thread_times


class ThreadTimesKeySilkCases(test.Test):
  """Measures timeline metrics while performing smoothness action on key silk
  cases."""
  test = thread_times.ThreadTimes
  page_set = 'page_sets/key_silk_cases.py'
  options = {"report_silk_results": True}


class ThreadTimesFastPathKeySilkCases(test.Test):
  """Measures timeline metrics while performing smoothness action on key silk
  cases using bleeding edge rendering fast paths."""
  tag = 'fast_path'
  test = thread_times.ThreadTimes
  page_set = 'page_sets/key_silk_cases.py'
  options = {"report_silk_results": True}
  def CustomizeBrowserOptions(self, options):
    silk_flags.CustomizeBrowserOptionsForFastPath(options)


class LegacySilkBenchmark(ThreadTimesKeySilkCases):
  """Same as thread_times.key_silk_cases but with the old name."""
  @classmethod
  def GetName(cls):
    return "silk.key_silk_cases"

class ThreadTimesFastPathMobileSites(test.Test):
  """Measures timeline metrics while performing smoothness action on
  key mobile sites labeled with fast-path tag.
  http://www.chromium.org/developers/design-documents/rendering-benchmarks"""
  test = thread_times.ThreadTimes
  page_set = 'page_sets/key_mobile_sites.py'
  options = {'page_label_filter' : 'fastpath'}

class LegacyFastPathBenchmark(ThreadTimesFastPathMobileSites):
  """Same as thread_times.fast_path_mobile_sites but with the old name."""
  @classmethod
  def GetName(cls):
    return "fast_path.key_mobile_sites"
