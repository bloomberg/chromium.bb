# Copyright 2016 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import unittest

import loading_model
import request_track
import test_utils
import user_satisfied_lens

class UserSatisfiedLensTestCase(unittest.TestCase):
  def setUp(self):
    super(UserSatisfiedLensTestCase, self).setUp()
    self._request_index = 1

  def _RequestAt(self, timestamp_msec, duration=1):
    timestamp_sec = float(timestamp_msec) / 1000
    rq = request_track.Request.FromJsonDict({
        'url': 'http://bla-%s-.com' % timestamp_msec,
        'request_id': '1234.%s' % self._request_index,
        'frame_id': '123.%s' % timestamp_msec,
        'initiator': {'type': 'other'},
        'timestamp': timestamp_sec,
        'timing': request_track.TimingFromDict({
            'requestTime': timestamp_sec,
            'loadingFinished': duration})
        })
    self._request_index += 1
    return rq

  def testUserSatisfiedLens(self):
    # We track all times in milliseconds, but raw trace events are in
    # microseconds.
    MILLI_TO_MICRO = 1000
    loading_trace = test_utils.LoadingTraceFromEvents(
        [self._RequestAt(1), self._RequestAt(10), self._RequestAt(20)],
        trace_events=[{'ts': 0, 'ph': 'I',
                       'cat': 'blink.some_other_user_timing',
                       'name': 'firstContentfulPaint'},
                      {'ts': 9 * MILLI_TO_MICRO, 'ph': 'I',
                       'cat': 'blink.user_timing',
                       'name': 'firstDiscontentPaint'},
                      {'ts': 12 * MILLI_TO_MICRO, 'ph': 'I',
                       'cat': 'blink.user_timing',
                       'name': 'firstContentfulPaint'},
                      {'ts': 22 * MILLI_TO_MICRO, 'ph': 'I',
                       'cat': 'blink.user_timing',
                       'name': 'firstContentfulPaint'}])
    graph = loading_model.ResourceGraph(loading_trace)
    lens = user_satisfied_lens.UserSatisfiedLens(loading_trace, graph)
    for n in graph.Nodes():
      if n.Request().frame_id == '123.20':
        self.assertFalse(lens.Filter(n))
      else:
        self.assertTrue(lens.Filter(n))
