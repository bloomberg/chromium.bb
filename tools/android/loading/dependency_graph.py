# Copyright 2016 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Request dependency graph."""

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

  @property
  def graph(self):
    """Return the Graph we're based on."""
    return self._deps_graph

  def UpdateRequestsCost(self, request_id_to_cost):
    """Updates the cost of the nodes identified by their request ID.

    Args:
      request_id_to_cost: {request_id: new_cost} Can be a superset of the
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
