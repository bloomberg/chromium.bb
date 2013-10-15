# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from telemetry import test

from measurements import endure


class _EndureBenchmark(test.Test):
  test = endure.Endure
  # Default options for endure benchmarks. Could be overridden in subclasses.
  options = {
      'output_format': 'csv',
      'skip_navigate_on_repeat': True,
      'page_repeat_secs': 7200,
      'perf_stats_interval': '100s'
  }

class EndureCalendarForwardBackward(_EndureBenchmark):
  page_set = 'page_sets/calendar_forward_backward.json'


class EndureBrowserControl(_EndureBenchmark):
  page_set = 'page_sets/browser_control.json'


class EndureBrowserControlClick(_EndureBenchmark):
  page_set = 'page_sets/browser_control_click.json'


class EndureGmailAltThreadlistConversation(_EndureBenchmark):
  page_set = 'page_sets/gmail_alt_threadlist_conversation.json'


class EndureGmailAltTwoLabels(_EndureBenchmark):
  page_set = 'page_sets/gmail_alt_two_labels.json'


class EndureGmailExpandCollapseConversation(_EndureBenchmark):
  page_set = 'page_sets/gmail_expand_collapse_conversation.json'


class EndureIndexedDBOffline(_EndureBenchmark):
  page_set = 'page_sets/indexeddb_offline.json'


class EndurePlusAltPostsPhotos(_EndureBenchmark):
  page_set = 'page_sets/plus_alt_posts_photos.json'

