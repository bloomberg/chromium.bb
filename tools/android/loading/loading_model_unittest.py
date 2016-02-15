# Copyright 2015 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os
import sys
import unittest

import dag
import loading_model
import request_track
import request_dependencies_lens
import test_utils


class SimpleLens(object):
  def __init__(self, trace):
    self._trace = trace

  def GetRequestDependencies(self):
    url_to_rq = {}
    deps = []
    for rq in self._trace.request_track.GetEvents():
      assert rq.url not in url_to_rq
      url_to_rq[rq.url] = rq
    for rq in self._trace.request_track.GetEvents():
      initiating_url = rq.initiator['url']
      if initiating_url in url_to_rq:
        deps.append((url_to_rq[initiating_url], rq, rq.initiator['type']))
    return deps


class LoadingModelTestCase(unittest.TestCase):

  def setUp(self):
    self.old_lens = request_dependencies_lens.RequestDependencyLens
    request_dependencies_lens.RequestDependencyLens = SimpleLens

  def tearDown(self):
    request_dependencies_lens.RequestDependencyLens = self.old_lens

  def MakeGraph(self, requests):
    return loading_model.ResourceGraph(
        test_utils.LoadingTraceFromEvents(requests))

  def SortedIndicies(self, graph):
    return [n.Index() for n in dag.TopologicalSort(graph._nodes)]

  def SuccessorIndicies(self, node):
    return [c.Index() for c in node.SortedSuccessors()]

  def test_DictConstruction(self):
    graph = loading_model.ResourceGraph(
        {'request_track': {
            'events': [
                test_utils.MakeRequest(0, 'null', 100, 100.5, 101).ToJsonDict(),
                test_utils.MakeRequest(1, 0, 102, 102.5, 103).ToJsonDict(),
                test_utils.MakeRequest(2, 0, 102, 102.5, 103).ToJsonDict(),
                test_utils.MakeRequest(3, 2, 104, 114.5, 105).ToJsonDict()],
            'metadata': {
                request_track.RequestTrack._DUPLICATES_KEY: 0,
                request_track.RequestTrack._INCONSISTENT_INITIATORS_KEY: 0}},
         'url': 'foo.com',
         'tracing_track': {'events': []},
         'page_track': {'events': []},
         'metadata': {}})
    self.assertEqual(self.SuccessorIndicies(graph._nodes[0]), [1, 2])
    self.assertEqual(self.SuccessorIndicies(graph._nodes[1]), [])
    self.assertEqual(self.SuccessorIndicies(graph._nodes[2]), [3])
    self.assertEqual(self.SuccessorIndicies(graph._nodes[3]), [])

  def test_Costing(self):
    requests = [test_utils.MakeRequest(0, 'null', 100, 105, 110),
                test_utils.MakeRequest(1, 0, 115, 117, 120),
                test_utils.MakeRequest(2, 0, 112, 116, 120),
                test_utils.MakeRequest(3, 1, 122, 124, 126),
                test_utils.MakeRequest(4, 3, 127, 127.5, 128),
                test_utils.MakeRequest(5, 'null', 100, 103, 105),
                test_utils.MakeRequest(6, 5, 105, 107, 110)]
    graph = self.MakeGraph(requests)
    self.assertEqual(self.SuccessorIndicies(graph._nodes[0]), [1, 2])
    self.assertEqual(self.SuccessorIndicies(graph._nodes[1]), [3])
    self.assertEqual(self.SuccessorIndicies(graph._nodes[2]), [])
    self.assertEqual(self.SuccessorIndicies(graph._nodes[3]), [4])
    self.assertEqual(self.SuccessorIndicies(graph._nodes[4]), [])
    self.assertEqual(self.SuccessorIndicies(graph._nodes[5]), [6])
    self.assertEqual(self.SuccessorIndicies(graph._nodes[6]), [])
    self.assertEqual(self.SortedIndicies(graph), [0, 5, 1, 2, 6, 3, 4])
    self.assertEqual(28, graph.Cost())
    graph.Set(cache_all=True)
    self.assertEqual(8, graph.Cost())

  def test_MaxPath(self):
    requests = [test_utils.MakeRequest(0, 'null', 100, 110, 111),
                test_utils.MakeRequest(1, 0, 115, 120, 121),
                test_utils.MakeRequest(2, 0, 112, 120, 121),
                test_utils.MakeRequest(3, 1, 122, 126, 127),
                test_utils.MakeRequest(4, 3, 127, 128, 129),
                test_utils.MakeRequest(5, 'null', 100, 105, 106),
                test_utils.MakeRequest(6, 5, 105, 110, 111)]
    graph = self.MakeGraph(requests)
    path_list = []
    self.assertEqual(29, graph.Cost(path_list))
    self.assertEqual([0, 1, 3, 4], [n.Index() for n in path_list])

    # More interesting would be a test when a node has multiple predecessors,
    # but it's not possible for us to construct such a graph from requests yet.

  def test_TimingSplit(self):
    # Timing adds node 1 as a parent to 2 but not 3.
    requests = [
        test_utils.MakeRequest(0, 'null', 100, 110, 110,
                               magic_content_type=True),
        test_utils.MakeRequest(1, 0, 115, 120, 120,
                               magic_content_type=True),
        test_utils.MakeRequest(2, 0, 121, 122, 122,
                               magic_content_type=True),
        test_utils.MakeRequest(3, 0, 112, 119, 119,
                               magic_content_type=True),
        test_utils.MakeRequest(4, 2, 122, 126, 126),
        test_utils.MakeRequest(5, 2, 122, 126, 126)]
    graph = self.MakeGraph(requests)
    self.assertEqual(self.SuccessorIndicies(graph._nodes[0]), [1, 3])
    self.assertEqual(self.SuccessorIndicies(graph._nodes[1]), [2])
    self.assertEqual(self.SuccessorIndicies(graph._nodes[2]), [4, 5])
    self.assertEqual(self.SuccessorIndicies(graph._nodes[3]), [])
    self.assertEqual(self.SuccessorIndicies(graph._nodes[4]), [])
    self.assertEqual(self.SuccessorIndicies(graph._nodes[5]), [])
    self.assertEqual(self.SortedIndicies(graph), [0, 1, 3, 2, 4, 5])

    # Change node 1 so it is a parent of 3, which becomes the parent of 2.
    requests[1] = test_utils.MakeRequest(
        1, 0, 110, 111, 111, magic_content_type=True)
    graph = self.MakeGraph(requests)
    self.assertEqual(self.SuccessorIndicies(graph._nodes[0]), [1])
    self.assertEqual(self.SuccessorIndicies(graph._nodes[1]), [3])
    self.assertEqual(self.SuccessorIndicies(graph._nodes[2]), [4, 5])
    self.assertEqual(self.SuccessorIndicies(graph._nodes[3]), [2])
    self.assertEqual(self.SuccessorIndicies(graph._nodes[4]), [])
    self.assertEqual(self.SuccessorIndicies(graph._nodes[5]), [])
    self.assertEqual(self.SortedIndicies(graph), [0, 1, 3, 2, 4, 5])

    # Add an initiator dependence to 1 that will become the parent of 3.
    requests[1] = test_utils.MakeRequest(
        1, 0, 110, 111, 111, magic_content_type=True)
    requests.append(test_utils.MakeRequest(6, 1, 111, 112, 112))
    graph = self.MakeGraph(requests)
    # Check it doesn't change until we change the content type of 6.
    self.assertEqual(self.SuccessorIndicies(graph._nodes[6]), [])
    requests[6] = test_utils.MakeRequest(6, 1, 111, 112, 112,
                                         magic_content_type=True)
    graph = self.MakeGraph(requests)
    self.assertEqual(self.SuccessorIndicies(graph._nodes[0]), [1])
    self.assertEqual(self.SuccessorIndicies(graph._nodes[1]), [6])
    self.assertEqual(self.SuccessorIndicies(graph._nodes[2]), [4, 5])
    self.assertEqual(self.SuccessorIndicies(graph._nodes[3]), [2])
    self.assertEqual(self.SuccessorIndicies(graph._nodes[4]), [])
    self.assertEqual(self.SuccessorIndicies(graph._nodes[5]), [])
    self.assertEqual(self.SuccessorIndicies(graph._nodes[6]), [3])
    self.assertEqual(self.SortedIndicies(graph), [0, 1, 6, 3, 2, 4, 5])

  def test_TimingSplitImage(self):
    # If we're all image types, then we shouldn't split by timing.
    requests = [test_utils.MakeRequest(0, 'null', 100, 110, 110),
                test_utils.MakeRequest(1, 0, 115, 120, 120),
                test_utils.MakeRequest(2, 0, 121, 122, 122),
                test_utils.MakeRequest(3, 0, 112, 119, 119),
                test_utils.MakeRequest(4, 2, 122, 126, 126),
                test_utils.MakeRequest(5, 2, 122, 126, 126)]
    for r in requests:
      r.response_headers['Content-Type'] = 'image/gif'
    graph = self.MakeGraph(requests)
    self.assertEqual(self.SuccessorIndicies(graph._nodes[0]), [1, 2, 3])
    self.assertEqual(self.SuccessorIndicies(graph._nodes[1]), [])
    self.assertEqual(self.SuccessorIndicies(graph._nodes[2]), [4, 5])
    self.assertEqual(self.SuccessorIndicies(graph._nodes[3]), [])
    self.assertEqual(self.SuccessorIndicies(graph._nodes[4]), [])
    self.assertEqual(self.SuccessorIndicies(graph._nodes[5]), [])
    self.assertEqual(self.SortedIndicies(graph), [0, 1, 2, 3, 4, 5])


if __name__ == '__main__':
  unittest.main()
