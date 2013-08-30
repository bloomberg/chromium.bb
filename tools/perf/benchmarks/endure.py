# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from telemetry import test

from measurements import endure


class EndureCalendarForwardBackward(test.Test):
  test = endure.Endure
  page_set = 'page_sets/calendar_forward_backward.json'
  options = {
      'output_format': 'csv',
      'skip_navigate_on_repeat': True,
      'page_repeat_secs': 3600,
      'perf_stats_interval': 30
  }


class EndureGmailAltThreadlistConversation(test.Test):
  test = endure.Endure
  page_set = 'page_sets/gmail_alt_threadlist_conversation.json'
  options = {
      'output_format': 'csv',
      'skip_navigate_on_repeat': True,
      'page_repeat_secs': 3600,
      'perf_stats_interval': 30
  }


class EndureGmailAltTwoLabels(test.Test):
  test = endure.Endure
  page_set = 'page_sets/gmail_alt_two_labels.json'
  options = {
      'output_format': 'csv',
      'skip_navigate_on_repeat': True,
      'page_repeat_secs': 3600,
      'perf_stats_interval': 30
  }


class EndureGmailExpandCollapseConversation(test.Test):
  test = endure.Endure
  page_set = 'page_sets/gmail_expand_collapse_conversation.json'
  options = {
      'output_format': 'csv',
      'skip_navigate_on_repeat': True,
      'page_repeat_secs': 3600,
      'perf_stats_interval': 30
  }


class EndurePlusAltPostsPhotos(test.Test):
  test = endure.Endure
  page_set = 'page_sets/plus_alt_posts_photos.json'
  options = {
      'output_format': 'csv',
      'skip_navigate_on_repeat': True,
      'page_repeat_secs': 3600,
      'perf_stats_interval': 30
  }

