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
      if rq.initiator in url_to_rq:
        deps.append(( url_to_rq[rq.initiator], rq, ''))
    return deps


class LoadingModelTestCase(unittest.TestCase):

  def setUp(self):
    request_dependencies_lens.RequestDependencyLens = SimpleLens
    self._next_request_id = 0

  def MakeParserRequest(self, url, source_url, start_time, end_time,
                        magic_content_type=False):
    timing = request_track.TimingAsList(request_track.TimingFromDict({
        # connectEnd should be ignored.
        'connectEnd': (end_time - start_time) / 2,
        'receiveHeadersEnd': end_time - start_time,
        'requestTime': start_time / 1000.0}))
    rq = request_track.Request.FromJsonDict({
        'timestamp': start_time / 1000.0,
        'request_id': self._next_request_id,
        'url': 'http://' + str(url),
        'initiator': 'http://' + str(source_url),
        'response_headers': {'Content-Type':
                             'null' if not magic_content_type
                             else 'magic-debug-content' },
        'timing': timing
        })
    self._next_request_id += 1
    return rq

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
            'events': [self.MakeParserRequest(0, 'null', 100, 101).ToJsonDict(),
                       self.MakeParserRequest(1, 0, 102, 103).ToJsonDict(),
                       self.MakeParserRequest(2, 0, 102, 103).ToJsonDict(),
                       self.MakeParserRequest(3, 2, 104, 105).ToJsonDict()],
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
    requests = [self.MakeParserRequest(0, 'null', 100, 110),
                self.MakeParserRequest(1, 0, 115, 120),
                self.MakeParserRequest(2, 0, 112, 120),
                self.MakeParserRequest(3, 1, 122, 126),
                self.MakeParserRequest(4, 3, 127, 128),
                self.MakeParserRequest(5, 'null', 100, 105),
                self.MakeParserRequest(6, 5, 105, 110)]
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
    requests = [self.MakeParserRequest(0, 'null', 100, 110),
                self.MakeParserRequest(1, 0, 115, 120),
                self.MakeParserRequest(2, 0, 112, 120),
                self.MakeParserRequest(3, 1, 122, 126),
                self.MakeParserRequest(4, 3, 127, 128),
                self.MakeParserRequest(5, 'null', 100, 105),
                self.MakeParserRequest(6, 5, 105, 110)]
    graph = self.MakeGraph(requests)
    path_list = []
    self.assertEqual(28, graph.Cost(path_list))
    self.assertEqual([0, 1, 3, 4], [n.Index() for n in path_list])

    # More interesting would be a test when a node has multiple predecessors,
    # but it's not possible for us to construct such a graph from requests yet.

  def test_TimingSplit(self):
    # Timing adds node 1 as a parent to 2 but not 3.
    requests = [self.MakeParserRequest(0, 'null', 100, 110,
                                       magic_content_type=True),
                self.MakeParserRequest(1, 0, 115, 120,
                                       magic_content_type=True),
                self.MakeParserRequest(2, 0, 121, 122,
                                       magic_content_type=True),
                self.MakeParserRequest(3, 0, 112, 119,
                                       magic_content_type=True),
                self.MakeParserRequest(4, 2, 122, 126),
                self.MakeParserRequest(5, 2, 122, 126)]
    graph = self.MakeGraph(requests)
    self.assertEqual(self.SuccessorIndicies(graph._nodes[0]), [1, 3])
    self.assertEqual(self.SuccessorIndicies(graph._nodes[1]), [2])
    self.assertEqual(self.SuccessorIndicies(graph._nodes[2]), [4, 5])
    self.assertEqual(self.SuccessorIndicies(graph._nodes[3]), [])
    self.assertEqual(self.SuccessorIndicies(graph._nodes[4]), [])
    self.assertEqual(self.SuccessorIndicies(graph._nodes[5]), [])
    self.assertEqual(self.SortedIndicies(graph), [0, 1, 3, 2, 4, 5])

    # Change node 1 so it is a parent of 3, which becomes the parent of 2.
    requests[1] = self.MakeParserRequest(1, 0, 110, 111,
                                         magic_content_type=True)
    graph = self.MakeGraph(requests)
    self.assertEqual(self.SuccessorIndicies(graph._nodes[0]), [1])
    self.assertEqual(self.SuccessorIndicies(graph._nodes[1]), [3])
    self.assertEqual(self.SuccessorIndicies(graph._nodes[2]), [4, 5])
    self.assertEqual(self.SuccessorIndicies(graph._nodes[3]), [2])
    self.assertEqual(self.SuccessorIndicies(graph._nodes[4]), [])
    self.assertEqual(self.SuccessorIndicies(graph._nodes[5]), [])
    self.assertEqual(self.SortedIndicies(graph), [0, 1, 3, 2, 4, 5])

    # Add an initiator dependence to 1 that will become the parent of 3.
    requests[1] = self.MakeParserRequest(1, 0, 110, 111,
                                         magic_content_type=True)
    requests.append(self.MakeParserRequest(6, 1, 111, 112))
    graph = self.MakeGraph(requests)
    # Check it doesn't change until we change the content type of 6.
    self.assertEqual(self.SuccessorIndicies(graph._nodes[6]), [])
    requests[6] = self.MakeParserRequest(6, 1, 111, 112,
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
    requests = [self.MakeParserRequest(0, 'null', 100, 110),
                self.MakeParserRequest(1, 0, 115, 120),
                self.MakeParserRequest(2, 0, 121, 122),
                self.MakeParserRequest(3, 0, 112, 119),
                self.MakeParserRequest(4, 2, 122, 126),
                self.MakeParserRequest(5, 2, 122, 126)]
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
