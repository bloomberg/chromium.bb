# Copyright 2016 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import unittest

import dependency_graph
import request_dependencies_lens
from request_dependencies_lens_unittest import TestRequests
import request_track


class RequestDependencyGraphTestCase(unittest.TestCase):
  def setUp(self):
    super(RequestDependencyGraphTestCase, self).setUp()
    self.trace = TestRequests.CreateLoadingTrace()

  def testUpdateRequestCost(self):
    requests = self.trace.request_track.GetEvents()
    requests[0].timing = request_track.TimingFromDict(
        {'requestTime': 12, 'loadingFinished': 10})
    dependencies_lens = request_dependencies_lens.RequestDependencyLens(
        self.trace)
    g = dependency_graph.RequestDependencyGraph(requests, dependencies_lens)
    self.assertEqual(10, g.Cost())
    request_id = requests[0].request_id
    g.UpdateRequestsCost({request_id: 100})
    self.assertEqual(100, g.Cost())
    g.UpdateRequestsCost({'unrelated_id': 1000})
    self.assertEqual(100, g.Cost())

  def testCost(self):
    requests = self.trace.request_track.GetEvents()
    for (index, request) in enumerate(requests):
      request.timing = request_track.TimingFromDict(
          {'requestTime': index, 'receiveHeadersEnd': 10,
           'loadingFinished': 10})
    dependencies_lens = request_dependencies_lens.RequestDependencyLens(
        self.trace)
    g = dependency_graph.RequestDependencyGraph(requests, dependencies_lens)
    # First redirect -> Second redirect -> Redirected Request -> Request ->
    # JS Request 2
    self.assertEqual(7010, g.Cost())
    # Not on the critical path
    g.UpdateRequestsCost({TestRequests.JS_REQUEST.request_id: 0})
    self.assertEqual(7010, g.Cost())
    g.UpdateRequestsCost({TestRequests.FIRST_REDIRECT_REQUEST.request_id: 0})
    self.assertEqual(7000, g.Cost())
    g.UpdateRequestsCost({TestRequests.SECOND_REDIRECT_REQUEST.request_id: 0})
    self.assertEqual(6990, g.Cost())


if __name__ == '__main__':
  unittest.main()
