# Copyright 2015 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os
import sys
import unittest

import dag
import loading_model
import log_parser

class LoadingModelTestCase(unittest.TestCase):

  def MakeParserRequest(self, url, source_url, start_time, end_time,
                        magic_content_type=False):
    timing_data = {f: -1 for f in log_parser.Timing._fields}
    # We should ignore connectEnd.
    timing_data['connectEnd'] = (end_time - start_time) / 2
    timing_data['receiveHeadersEnd'] = end_time - start_time
    timing_data['requestTime'] = start_time / 1000.0
    return log_parser.RequestData(
        None, {'Content-Type': 'null' if not magic_content_type
                                      else 'magic-debug-content' },
        None, start_time, timing_data, 'http://' + str(url), False,
        {'type': 'parser', 'url': 'http://' + str(source_url)})

  def SortedIndicies(self, graph):
    return [n.Index() for n in dag.TopologicalSort(graph._nodes)]

  def SuccessorIndicies(self, node):
    return [c.Index() for c in node.SortedSuccessors()]

  def test_Costing(self):
    requests = [self.MakeParserRequest(0, 'null', 100, 110),
                self.MakeParserRequest(1, 0, 115, 120),
                self.MakeParserRequest(2, 0, 112, 120),
                self.MakeParserRequest(3, 1, 122, 126),
                self.MakeParserRequest(4, 3, 127, 128),
                self.MakeParserRequest(5, 'null', 100, 105),
                self.MakeParserRequest(6, 5, 105, 110)]
    graph = loading_model.ResourceGraph(requests)
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
    graph = loading_model.ResourceGraph(requests)
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
                self.MakeParserRequest(3, 0, 112, 119),
                self.MakeParserRequest(4, 2, 122, 126),
                self.MakeParserRequest(5, 2, 122, 126)]
    graph = loading_model.ResourceGraph(requests)
    self.assertEqual(self.SuccessorIndicies(graph._nodes[0]), [1, 3])
    self.assertEqual(self.SuccessorIndicies(graph._nodes[1]), [2])
    self.assertEqual(self.SuccessorIndicies(graph._nodes[2]), [4, 5])
    self.assertEqual(self.SuccessorIndicies(graph._nodes[3]), [])
    self.assertEqual(self.SuccessorIndicies(graph._nodes[4]), [])
    self.assertEqual(self.SuccessorIndicies(graph._nodes[5]), [])
    self.assertEqual(self.SortedIndicies(graph), [0, 1, 3, 2, 4, 5])

    # Change node 1 so it is a parent of 3, which become parent of 2.
    requests[1] = self.MakeParserRequest(1, 0, 110, 111,
                                         magic_content_type=True)
    graph = loading_model.ResourceGraph(requests)
    self.assertEqual(self.SuccessorIndicies(graph._nodes[0]), [1])
    self.assertEqual(self.SuccessorIndicies(graph._nodes[1]), [3])
    self.assertEqual(self.SuccessorIndicies(graph._nodes[2]), [4, 5])
    self.assertEqual(self.SuccessorIndicies(graph._nodes[3]), [2])
    self.assertEqual(self.SuccessorIndicies(graph._nodes[4]), [])
    self.assertEqual(self.SuccessorIndicies(graph._nodes[5]), [])
    self.assertEqual(self.SortedIndicies(graph), [0, 1, 3, 2, 4, 5])

    # Add an initiator dependence to 1 that will become the parent of 3.
    requests[1] = self.MakeParserRequest(1, 0, 110, 111)
    requests.append(self.MakeParserRequest(6, 1, 111, 112))
    graph = loading_model.ResourceGraph(requests)
    # Check it doesn't change until we change the content type of 1.
    self.assertEqual(self.SuccessorIndicies(graph._nodes[1]), [3, 6])
    requests[1] = self.MakeParserRequest(1, 0, 110, 111,
                                         magic_content_type=True)
    graph = loading_model.ResourceGraph(requests)
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
      r.headers['Content-Type'] = 'image/gif'
    graph = loading_model.ResourceGraph(requests)
    self.assertEqual(self.SuccessorIndicies(graph._nodes[0]), [1, 2, 3])
    self.assertEqual(self.SuccessorIndicies(graph._nodes[1]), [])
    self.assertEqual(self.SuccessorIndicies(graph._nodes[2]), [4, 5])
    self.assertEqual(self.SuccessorIndicies(graph._nodes[3]), [])
    self.assertEqual(self.SuccessorIndicies(graph._nodes[4]), [])
    self.assertEqual(self.SuccessorIndicies(graph._nodes[5]), [])
    self.assertEqual(self.SortedIndicies(graph), [0, 1, 2, 3, 4, 5])

  def test_AdUrl(self):
    self.assertTrue(loading_model.ResourceGraph._IsAdUrl(
        'http://afae61024b33032ef.profile.sfo20.cloudfront.net/test.png'))
    self.assertFalse(loading_model.ResourceGraph._IsAdUrl(
        'http://afae61024b33032ef.profile.sfo20.cloudfront.net/tst.png'))

    self.assertTrue(loading_model.ResourceGraph._IsAdUrl(
        'http://ums.adtechus.com/mapuser?providerid=1003;userid=RUmecco4z3o===='))
    self.assertTrue(loading_model.ResourceGraph._IsAdUrl(
        'http://pagead2.googlesyndication.com/pagead/js/adsbygoogle.js'))


if __name__ == '__main__':
  unittest.main()
