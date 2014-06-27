# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""The Endure benchmarks measure memory performance over a period of time.

In each Endure benchmark, one page action is performed repeatedly and memory
usage is measured periodically. The specific page actions are defined in the
page sets, and the statistics that are gathered are determined by the Endure
measurement class.
"""

from measurements import endure
import page_sets
from telemetry import benchmark


class _EndureBenchmark(benchmark.Benchmark):
  """Base class which sets options for endure benchmarks below."""
  test = endure.Endure
  # Default options for endure benchmarks. Could be overridden in subclasses.
  options = {
      # Depending on the page and the actions performed on the page,
      # 1000 iterations should be between 30 and 60 minutes.
      'page_repeat': 1000,
      # One sample per 10 iterations -> 200 points per run.
      'perf_stats_interval': 10
  }


@benchmark.Disabled
class EndureCalendarForwardBackward(_EndureBenchmark):
  page_set = page_sets.CalendarForwardBackwardPageSet


@benchmark.Disabled
class EndureBrowserControl(_EndureBenchmark):
  page_set = page_sets.BrowserControlPageSet


@benchmark.Disabled
class EndureBrowserControlClick(_EndureBenchmark):
  page_set = page_sets.BrowserControlClickPageSet


@benchmark.Disabled
class EndureGmailAltThreadlistConversation(_EndureBenchmark):
  page_set = page_sets.GmailAltThreadlistConversationPageSet


@benchmark.Disabled
class EndureGmailAltTwoLabels(_EndureBenchmark):
  page_set = page_sets.GmailAltTwoLabelsPageSet


@benchmark.Disabled
class EndureGmailExpandCollapseConversation(_EndureBenchmark):
  page_set = page_sets.GmailExpandCollapseConversationPageSet


@benchmark.Disabled
class EndureIndexedDBOffline(_EndureBenchmark):
  page_set = page_sets.IndexeddbOfflinePageSet


@benchmark.Disabled
class EndurePlusAltPostsPhotos(_EndureBenchmark):
  page_set = page_sets.PlusAltPostsPhotosPageSet

