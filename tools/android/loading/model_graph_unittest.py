# Copyright 2015 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import gzip
import json
import os.path
import tempfile
import unittest

import frame_load_lens
import loading_model
import loading_trace
import model_graph

TEST_DATA_DIR = os.path.join(os.path.dirname(__file__), 'testdata')

class ModelGraphTestCase(unittest.TestCase):
  _ROLLING_STONE = os.path.join(TEST_DATA_DIR, 'rollingstone.trace.gz')

  def test_EndToEnd(self):
    # Test that we don't crash. This also runs through frame_load_lens.
    tmp = tempfile.NamedTemporaryFile()
    with gzip.GzipFile(self._ROLLING_STONE) as f:
      trace = loading_trace.LoadingTrace.FromJsonDict(json.load(f))
      frame_lens = frame_load_lens.FrameLoadLens(trace)
      graph = loading_model.ResourceGraph(trace=trace, frame_lens=frame_lens)
      visualization = model_graph.GraphVisualization(graph)
      visualization.OutputDot(tmp)
