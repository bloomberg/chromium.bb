# Copyright 2016 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Request dependency graph."""

import logging
import sys

import graph
import request_track


class _RequestNode(graph.Node):
  def __init__(self, request):
    super(_RequestNode, self).__init__()
    self.request = request
    self.cost = request.Cost()


class _Edge(graph.Edge):
  def __init__(self, from_node, to_node, reason):
    super(_Edge, self).__init__(from_node, to_node)
    self.reason = reason
    self.cost = request_track.TimeBetween(
        self.from_node.request, self.to_node.request, self.reason)


class RequestDependencyGraph(object):
  """Request dependency graph."""
  # This resource type may induce a timing dependency. See _SplitChildrenByTime
  # for details.
  # TODO(lizeb,mattcary): are these right?
  _CAN_BE_TIMING_PARENT = set(['script', 'magic-debug-content'])
  _CAN_MAKE_TIMING_DEPENDENCE = set(['json', 'other', 'magic-debug-content'])

  def __init__(self, requests, dependencies_lens):
    """Creates a request dependency graph.

    Args:
      requests: ([Request]) a list of requests.
      dependencies_lens: (RequestDependencyLens)
    """
    self._requests = requests
    deps = dependencies_lens.GetRequestDependencies()
    self._nodes_by_id = {r.request_id : _RequestNode(r) for r in self._requests}
    edges = []
    for (parent_request, child_request, reason) in deps:
      if (parent_request.request_id not in self._nodes_by_id
          or child_request.request_id not in self._nodes_by_id):
        continue
      parent_node = self._nodes_by_id[parent_request.request_id]
      child_node = self._nodes_by_id[child_request.request_id]
      edges.append(_Edge(parent_node, child_node, reason))
    self._first_request_node = self._nodes_by_id[self._requests[0].request_id]
    self._deps_graph = graph.DirectedGraph(self._nodes_by_id.values(), edges)
    self._HandleTimingDependencies()

  @property
  def graph(self):
    """Return the Graph we're based on."""
    return self._deps_graph

  def UpdateRequestsCost(self, request_id_to_cost):
    """Updates the cost of the nodes identified by their request ID.

    Args:
      request_id_to_cost: ({request_id: new_cost}) Can be a superset of the
                          requests actually present in the graph.
    """
    for node in self._deps_graph.Nodes():
      request_id = node.request.request_id
      if request_id in request_id_to_cost:
        node.cost = request_id_to_cost[request_id]

  def Cost(self, from_first_request=True):
    """Returns the cost of the graph, that is the costliest path.

    Args:
      from_first_request: (boolean) If True, only considers paths that originate
                          from the first request node.
    """
    if from_first_request:
      return self._deps_graph.Cost([self._first_request_node])
    else:
      return self._deps_graph.Cost()

  def _HandleTimingDependencies(self):
    try:
      for n in self._deps_graph.TopologicalSort():
        self._SplitChildrenByTime(n)
    except AssertionError as exc:
      sys.stderr.write('Bad topological sort: %s\n'
                       'Skipping child split\n' % str(exc))

  def _SplitChildrenByTime(self, parent):
    """Splits children of a node by request times.

    The initiator of a request may not be the true dependency of a request. For
    example, a script may appear to load several resources independently, but in
    fact one of them may be a JSON data file, and the remaining resources assets
    described in the JSON. The assets should be dependent upon the JSON data
    file, and not the original script.

    This function approximates that by rearranging the children of a node
    according to their request times. The predecessor of each child is made to
    be the node with the greatest finishing time, that is before the start time
    of the child.

    We do this by sorting the nodes twice, once by start time and once by end
    time. We mark the earliest end time, and then we walk the start time list,
    advancing the end time mark when it is less than our current start time.

    This is refined by only considering assets which we believe actually create
    a dependency. We only split if the original parent is a script, and the new
    parent a data file.
    We incorporate this heuristic by skipping over any non-script/json resources
    when moving the end mark.

    TODO(mattcary): More heuristics, like incorporating cachability somehow, and
    not just picking arbitrarily if there are two nodes with the same end time
    (does that ever really happen?)

    Args:
      parent: (_RequestNode) The children of this node are processed by this
              function.
    """
    if parent.request.GetContentType() not in self._CAN_BE_TIMING_PARENT:
      return
    edges = self._deps_graph.OutEdges(parent)
    edges_by_start_time = sorted(
        edges, key=lambda e: e.to_node.request.start_msec)
    edges_by_end_time = sorted(
        edges, key=lambda e: e.to_node.request.end_msec)
    end_mark = 0
    for current in edges_by_start_time:
      assert current.from_node is parent
      if current.to_node.request.start_msec < parent.request.end_msec - 1e-5:
        parent_url = parent.request.url
        child_url = current.to_node.request.url
        logging.warning('Child loaded before parent finished: %s -> %s',
                        request_track.ShortName(parent_url),
                        request_track.ShortName(child_url))
      go_to_next_child = False
      while end_mark < len(edges_by_end_time):
        if edges_by_end_time[end_mark] == current:
          go_to_next_child = True
          break
        elif (edges_by_end_time[end_mark].to_node.request.GetContentType()
              not in self._CAN_MAKE_TIMING_DEPENDENCE):
          end_mark += 1
        elif (end_mark < len(edges_by_end_time) - 1 and
              edges_by_end_time[end_mark + 1].to_node.request.end_msec
              < current.to_node.request.start_msec):
          end_mark += 1
        else:
          break
      if end_mark >= len(edges_by_end_time):
        break  # It's not possible to rearrange any more children.
      if go_to_next_child:
        continue  # We can't rearrange this child, but the next child may be
                  # eligible.
      if (edges_by_end_time[end_mark].to_node.request.end_msec
          <= current.to_node.request.start_msec):
        self._deps_graph.UpdateEdge(
            current, edges_by_end_time[end_mark].to_node,
            current.to_node)
