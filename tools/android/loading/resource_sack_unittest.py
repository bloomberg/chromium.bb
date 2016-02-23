# Copyright 2016 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import unittest

import resource_sack
from test_utils import (MakeRequest,
                        TestResourceGraph)


class ResourceSackTestCase(unittest.TestCase):
  def test_NodeMerge(self):
    g1 = TestResourceGraph.FromRequestList([
        MakeRequest(0, 'null'),
        MakeRequest(1, 0),
        MakeRequest(2, 0),
        MakeRequest(3, 1)])
    g2 = TestResourceGraph.FromRequestList([
        MakeRequest(0, 'null'),
        MakeRequest(1, 0),
        MakeRequest(2, 0),
        MakeRequest(4, 2)])
    sack = resource_sack.GraphSack()
    sack.ConsumeGraph(g1)
    sack.ConsumeGraph(g2)
    self.assertEqual(5, len(sack.bags))
    for bag in sack.bags:
      if bag.label not in ('3/', '4/'):
        self.assertEqual(2, bag.num_nodes)
      else:
        self.assertEqual(1, bag.num_nodes)

  def test_MultiParents(self):
    g1 = TestResourceGraph.FromRequestList([
        MakeRequest(0, 'null'),
        MakeRequest(2, 0)])
    g2 = TestResourceGraph.FromRequestList([
        MakeRequest(1, 'null'),
        MakeRequest(2, 1)])
    sack = resource_sack.GraphSack()
    sack.ConsumeGraph(g1)
    sack.ConsumeGraph(g2)
    self.assertEqual(3, len(sack.bags))
    labels = {bag.label: bag for bag in sack.bags}
    self.assertEqual(
        set(['0/', '1/']),
        set([bag.label for bag in labels['2/'].Predecessors()]))
    self.assertFalse(labels['0/'].Predecessors())
    self.assertFalse(labels['1/'].Predecessors())

  def test_Shortname(self):
    root = MakeRequest(0, 'null')
    shortname = MakeRequest(1, 0)
    shortname.url = 'data:fake/content;' + 'lotsand' * 50 + 'lotsofdata'
    g1 = TestResourceGraph.FromRequestList([root, shortname])
    sack = resource_sack.GraphSack()
    sack.ConsumeGraph(g1)
    self.assertEqual(set(['0/', 'data:fake/content']),
                     set([bag.label for bag in sack.bags]))


if __name__ == '__main__':
  unittest.main()
