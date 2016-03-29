# Copyright 2016 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Support for graphs."""

import collections


class Node(object):
  """A node in a Graph.

  Nodes are identified within a graph using object identity.
  """
  def __init__(self):
    """Create a new node."""
    self.cost = 0


class Edge(object):
  """Represents an edge in a graph."""
  def __init__(self, from_node, to_node):
    """Creates an Edge.

    Args:
      from_node: (Node) Start node.
      to_node: (Node) End node.
    """
    self.from_node = from_node
    self.to_node = to_node
    self.cost = 0


class DirectedGraph(object):
  """Directed graph.

  A graph is identified by a list of nodes and a list of edges. It does not need
  to be acyclic, but then some methods will fail.
  """
  def __init__(self, nodes, edges):
    """Builds a graph from a set of node and edges.

    Note that the edges referencing a node not in the provided list are dropped.

    Args:
      nodes: ([Node]) Sequence of Nodes.
      edges: ([Edge]) Sequence of Edges.
    """
    self._nodes = set(nodes)
    self._edges = set(filter(
        lambda e: e.from_node in self._nodes and e.to_node in self._nodes,
        edges))
    assert all(isinstance(node, Node) for node in self._nodes)
    assert all(isinstance(edge, Edge) for edge in self._edges)
    self._in_edges = {n: [] for n in self._nodes}
    self._out_edges = {n: [] for n in self._nodes}
    for edge in self._edges:
      self._out_edges[edge.from_node].append(edge)
      self._in_edges[edge.to_node].append(edge)

  def OutEdges(self, node):
    """Returns a list of edges starting from a node.
    """
    return self._out_edges[node]

  def InEdges(self, node):
    """Returns a list of edges ending at a node."""
    return self._in_edges[node]

  def Nodes(self):
    """Returns the set of nodes of this graph."""
    return self._nodes

  def Edges(self):
    """Returns the set of edges of this graph."""
    return self._edges

  def UpdateEdge(self, edge, new_from_node, new_to_node):
    """Updates an edge.

    Args:
      edge:
      new_from_node:
      new_to_node:
    """
    assert edge in self._edges
    assert new_from_node in self._nodes
    assert new_to_node in self._nodes
    self._in_edges[edge.to_node].remove(edge)
    self._out_edges[edge.from_node].remove(edge)
    edge.from_node = new_from_node
    edge.to_node = new_to_node
    # TODO(lizeb): Check for duplicate edges?
    self._in_edges[edge.to_node].append(edge)
    self._out_edges[edge.from_node].append(edge)

  def TopologicalSort(self, roots=None):
    """Returns a list of nodes, in topological order.

      Args:
        roots: ([Node]) If set, the topological sort will only consider nodes
                        reachable from this list of sources.
    """
    sorted_nodes = []
    if roots is None:
      nodes_subset = self._nodes
    else:
      nodes_subset = self.ReachableNodes(roots)
    remaining_in_edges = {n: 0 for n in nodes_subset}
    for edge in self._edges:
      if edge.from_node in nodes_subset and edge.to_node in nodes_subset:
        remaining_in_edges[edge.to_node] += 1
    sources = [node for (node, count) in remaining_in_edges.items()
               if count == 0]
    while sources:
      node = sources.pop(0)
      sorted_nodes.append(node)
      for e in self.OutEdges(node):
        successor = e.to_node
        if successor not in nodes_subset:
          continue
        assert remaining_in_edges[successor] > 0
        remaining_in_edges[successor] -= 1
        if remaining_in_edges[successor] == 0:
          sources.append(successor)
    return sorted_nodes

  def ReachableNodes(self, roots):
    """Returns a list of nodes from a set of root nodes."""
    visited = set()
    fifo = collections.deque(roots)
    while len(fifo) != 0:
      node = fifo.pop()
      visited.add(node)
      for e in self.OutEdges(node):
        if e.to_node not in visited:
          visited.add(e.to_node)
        fifo.appendleft(e.to_node)
    return list(visited)

  def Cost(self, roots=None, path_list=None, costs_out=None):
    """Compute the cost of the graph.

    Args:
      roots: ([Node]) If set, only compute the cost of the paths reachable
             from this list of nodes.
      path_list: if not None, gets a list of nodes in the longest path.
      costs_out: if not None, gets a vector of node costs by node.

    Returns:
      Cost of the longest path.
    """
    costs = {n: 0 for n in self._nodes}
    for node in self.TopologicalSort(roots):
      cost = 0
      if self.InEdges(node):
        cost = max([costs[e.from_node] + e.cost for e in self.InEdges(node)])
      costs[node] = cost + node.cost
    max_cost = max(costs.values())
    if costs_out is not None:
      del costs_out[:]
      costs_out.extend(costs)
    assert max_cost > 0
    if path_list is not None:
      del path_list[:]
      node = (i for i in self._nodes if costs[i] == max_cost).next()
      path_list.append(node)
      while self.InEdges(node):
        predecessors = [e.from_node for e in self.InEdges(node)]
        node = reduce(
            lambda costliest_node, next_node:
            next_node if costs[next_node] > costs[costliest_node]
            else costliest_node, predecessors)
        path_list.insert(0, node)
    return max_cost
