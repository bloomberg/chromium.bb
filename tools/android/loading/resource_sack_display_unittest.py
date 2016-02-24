# Copyright 2016 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import re
from StringIO import StringIO
import unittest

import resource_sack
import resource_sack_display
from test_utils import (MakeRequest,
                        TestResourceGraph)


class ResourceSackDispayTestCase(unittest.TestCase):
  def test_SimpleOutput(self):
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
    buf = StringIO()
    resource_sack_display.ToDot(sack, buf,
                                long_edge_msec=1000)
    dot = buf.getvalue()
    # Short edge.
    self.assertTrue(re.search(r'0 -> 1[^]]+color=green \]', dot, re.MULTILINE))
    # Long edge.
    self.assertTrue(re.search(r'0 -> 3[^]]+penwidth=8', dot))



if __name__ == '__main__':
  unittest.main()
