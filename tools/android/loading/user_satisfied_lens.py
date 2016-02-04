# Copyright 2016 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Identifies a user satisfied event, and marks all the relationship of all
model events accordingly.
"""

import logging


class UserSatisfiedLens(object):
  def __init__(self, trace, graph):
    """Initialize the lens.

    Args:
      trace: (LoadingTrace) the trace to use in the analysis.
      graph: (ResourceGraph) the graph to annotate, using the current filter set
        on the graph.
    """
    satisfied_time = self._GetFirstContentfulPaintTime(trace.tracing_track)
    self._satisfied_nodes = set(self._GetNodesAfter(
        graph.Nodes(), satisfied_time))

  def Filter(self, node):
    """A ResourceGraph filter.

    Meant to be used in some_graph.Set(node_filter=this_lens.Filter).

    Args:
      node: a ResourceGraph NodeInfo.

    Returns:
      True iff the node occurred before user satisfaction occurred.
    """
    return node not in self._satisfied_nodes

  # TODO(mattcary): hoist to base class?
  @classmethod
  def _GetNodesAfter(cls, nodes, time):
    return (n for n in nodes if n.StartTime() >= time)

  def _GetFirstContentfulPaintTime(self, tracing_track):
    first_paints = [e.start_msec for e in tracing_track.GetEvents()
                    if (e.tracing_event['name'] == 'firstContentfulPaint' and
                        'blink.user_timing' in e.tracing_event['cat'])]
    if len(first_paints) != 1:
      # TODO(mattcary): in some cases a trace has two contentful paints. Why?
      logging.error('%d first paints with spread of %s', len(first_paints),
                    max(first_paints) - min(first_paints))
    return min(first_paints)
