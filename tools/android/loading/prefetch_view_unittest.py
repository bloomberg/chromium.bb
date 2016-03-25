# Copyright 2016 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import unittest

import prefetch_view
import request_dependencies_lens
from request_dependencies_lens_unittest import TestRequests
import request_track
import test_utils


class PrefetchSimulationViewTestCase(unittest.TestCase):
  def setUp(self):
    super(PrefetchSimulationViewTestCase, self).setUp()
    self._SetUp()

  def testExpandRedirectChains(self):
    self.assertListEqual(
        [TestRequests.FIRST_REDIRECT_REQUEST,
         TestRequests.SECOND_REDIRECT_REQUEST, TestRequests.REDIRECTED_REQUEST],
        self.prefetch_view.ExpandRedirectChains(
            [TestRequests.FIRST_REDIRECT_REQUEST]))

  def testParserDiscoverableRequests(self):
    first_request = TestRequests.FIRST_REDIRECT_REQUEST
    discovered_requests = self.prefetch_view.ParserDiscoverableRequests(
        first_request)
    self.assertListEqual(
        [TestRequests.FIRST_REDIRECT_REQUEST,
         TestRequests.JS_REQUEST, TestRequests.JS_REQUEST_OTHER_FRAME,
         TestRequests.JS_REQUEST_UNRELATED_FRAME], discovered_requests)

  def testPreloadedRequests(self):
    first_request = TestRequests.FIRST_REDIRECT_REQUEST
    preloaded_requests = self.prefetch_view.PreloadedRequests(first_request)
    self.assertListEqual([first_request], preloaded_requests)
    self._SetUp(
        [{'args': {'url': 'http://bla.com/nyancat.js'},
          'cat': 'blink.net', 'id': '0xaf9f14fa9dd6c314', 'name': 'Resource',
          'ph': 'X', 'ts': 1, 'dur': 120, 'pid': 12, 'tid': 12},
         {'args': {'step': 'Preload'}, 'cat': 'blink.net',
          'id': '0xaf9f14fa9dd6c314', 'name': 'Resource', 'ph': 'T',
          'ts': 12, 'pid': 12, 'tid': 12}])
    preloaded_requests = self.prefetch_view.PreloadedRequests(first_request)
    self.assertListEqual([TestRequests.FIRST_REDIRECT_REQUEST,
         TestRequests.JS_REQUEST, TestRequests.JS_REQUEST_OTHER_FRAME,
         TestRequests.JS_REQUEST_UNRELATED_FRAME], preloaded_requests)

  def _SetUp(self, added_trace_events=None):
    trace_events = [
        {'ts': 5, 'ph': 'X', 'dur': 10, 'pid': 2, 'tid': 1, 'cat': 'blink.net'}]
    if added_trace_events is not None:
      trace_events += added_trace_events
    self.trace = TestRequests.CreateLoadingTrace(trace_events)
    dependencies_lens = request_dependencies_lens.RequestDependencyLens(
        self.trace)
    self.user_satisfied_lens = test_utils.MockUserSatisfiedLens(self.trace)
    self.prefetch_view = prefetch_view.PrefetchSimulationView(
        self.trace, dependencies_lens, self.user_satisfied_lens)


if __name__ == '__main__':
  unittest.main()
