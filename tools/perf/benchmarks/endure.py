# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""The Endure benchmarks measure memory performance over a period of time.

In each Endure benchmark, one page action is performed repeatedly and memory
usage is measured periodically. The specific page actions are defined in the
page sets, and the statistics that are gathered are determined by the Endure
measurement class.
"""

from telemetry import test

from measurements import endure


class _EndureBenchmark(test.Test):
  """Base class which sets options for endure benchmarks below."""
  test = endure.Endure
  # Default options for endure benchmarks. Could be overridden in subclasses.
  options = {
      # Depending on the page and the actions performed on the page,
      # 2000 iterations should be between 1 and 2 hours.
      'page_repeat': 2000,
      # One sample per 10 iterations -> 200 points per run.
      'perf_stats_interval': 10
  }


class EndureCalendarForwardBackward(_EndureBenchmark):
  page_set = 'page_sets/calendar_forward_backward.py'


class EndureBrowserControl(_EndureBenchmark):
  page_set = 'page_sets/browser_control.py'


class EndureBrowserControlClick(_EndureBenchmark):
  page_set = 'page_sets/browser_control_click.py'


class EndureGmailAltThreadlistConversation(_EndureBenchmark):
  page_set = 'page_sets/gmail_alt_threadlist_conversation.py'


class EndureGmailAltTwoLabels(_EndureBenchmark):
  page_set = 'page_sets/gmail_alt_two_labels.py'


class EndureGmailExpandCollapseConversation(_EndureBenchmark):
  page_set = 'page_sets/gmail_expand_collapse_conversation.py'


class EndureIndexedDBOffline(_EndureBenchmark):
  page_set = 'page_sets/indexeddb_offline.py'


class EndurePlusAltPostsPhotos(_EndureBenchmark):
  page_set = 'page_sets/plus_alt_posts_photos.py'


class EndureGmailRefresh(test.Test):
  test = endure.Endure
  # Options for endure gmail page refresh benchmark test.
  options = {
      'page_repeat': 20,
      'perf_stats_interval': 1
  }
  page_set = 'page_sets/endure_gmail_refresh.py'
