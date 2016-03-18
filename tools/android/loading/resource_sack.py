# Copyright 2016 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""A collection of ResourceGraphs.

Processes multiple ResourceGraphs, all presumably from requests to the same
site. Common urls are collected in Bags and different statistics on the
relationship between bags are collected.
"""

import collections
import json
import sys
import urlparse

from collections import defaultdict

import content_classification_lens
import dag
import user_satisfied_lens

class GraphSack(object):
  """Aggreate of ResourceGraphs.

  Collects ResourceGraph nodes into bags, where each bag contains the nodes with
  common urls. Dependency edges are tracked between bags (so that each bag may
  be considered as a node of a graph). This graph of bags is referred to as a
  sack.

  Each bag is associated with a dag.Node, even though the bag graph may not be a
  DAG. The edges are annotated with list of graphs and nodes that generated
  them.
  """
  # See CoreSet().
  CORE_THRESHOLD = 0.8

  _GraphInfo = collections.namedtuple('_GraphInfo', (
      'cost',   # The graph cost (aka critical path length).
      'total_costs',  # A vector by node index of total cost of each node.
      ))

  def __init__(self):
    # A bag is a node in our combined graph.
    self._bags = []
    # Each bag in our sack corresponds to a url, as expressed by this map.
    self._url_to_bag = {}
    # Maps graph -> _GraphInfo structures for each graph we've consumed.
    self._graph_info = {}

  def ConsumeGraph(self, graph):
    """Add a graph and process.

    Args:
      graph: (ResourceGraph) the graph to add. The graph is processed sorted
        according to its current filter.
    """
    assert graph not in self._graph_info
    critical_path = []
    total_costs = []
    cost = graph.Cost(path_list=critical_path,
                      costs_out=total_costs)
    self._graph_info[graph] = self._GraphInfo(
        cost=cost, total_costs=total_costs)
    for n in graph.Nodes(sort=True):
      assert graph._node_filter(n.Node())
      self.AddNode(graph, n)
    for node in critical_path:
      self._url_to_bag[node.Url()].MarkCritical()

  def AddNode(self, graph, node):
    """Add a node to our collection.

    Args:
      graph: (ResourceGraph) the graph in which the node lives.
      node: (NodeInfo) the node to add.

    Returns:
      The Bag containing the node.
    """
    if not graph._node_filter(node):
      return
    if node.Url() not in self._url_to_bag:
      new_index = len(self._bags)
      self._bags.append(Bag(self, new_index, node.Url()))
      self._url_to_bag[node.Url()] = self._bags[-1]
    self._url_to_bag[node.Url()].AddNode(graph, node)
    return self._url_to_bag[node.Url()]

  def CoreSet(self, *graph_sets):
    """Compute the core set of this sack.

    The core set of a sack is the set of resource that are common to most of the
    graphs in the sack. A core set of a set of graphs are the resources that
    appear with frequency at least CORE_THRESHOLD. For a collection of graph
    sets, for instance pulling the same page under different network
    connections, we intersect the core sets to produce a page core set that
    describes the key resources used by the page. See https://goo.gl/F1BoEB for
    context and discussion.

    Args:
      graph_sets: one or more collection of graphs to compute core sets. If one
        graph set is given, its core set is computed. If more than one set is
        given, the page core set of all sets is computed (the intersection of
        core sets). If no graph set is given, the core of all graphs is
        computed.

    Returns:
      A set of bag labels (as strings) in the core set.
    """
    if not graph_sets:
      graph_sets = [self._graph_info.keys()]
    return reduce(lambda a, b: a & b,
                  (self._SingleCore(s) for s in graph_sets))

  @classmethod
  def CoreSimilarity(cls, a, b):
    """Compute the similarity of two core sets.

    We use the Jaccard index. See https://goo.gl/F1BoEB for discussion.

    Args:
      a: The first core set, as a set of strings.
      b: The second core set, as a set of strings.

    Returns:
      A similarity score between zero and one. If both sets are empty the
      similarity is zero.
    """
    if not a and not b:
      return 0
    return float(len(a & b)) / len(a | b)

  def FilterOccurrence(self, tag, filter_from_graph):
    """Accumulate filter occurrences for each bag in the graph.

    This can be retrieved under tag for each Bag in the graph. For example, if
    FilterContentful marks the nodes of each graph before the first contentful
    paint, then FilterOccurrence('contentful', FilterContentful) will count, for
    each bag, the fraction of nodes that were before the first contentful paint.

    Args:
      tag: the tag to count the filter appearances under.
      filter_from_graph: a function graph -> node filter, where node filter
        takes a node to a boolean.
    """
    for bag in self.bags:
      bag.MarkOccurrence(tag, filter_from_graph)

  @property
  def num_graphs(self):
    return len(self.graph_info)

  @property
  def graph_info(self):
    return self._graph_info

  @property
  def bags(self):
    return self._bags

  def _SingleCore(self, graph_set):
    core = set()
    graph_set = set(graph_set)
    num_graphs = len(graph_set)
    for b in self.bags:
      count = sum([g in graph_set for g in b.graphs])
      if float(count) / num_graphs > self.CORE_THRESHOLD:
        core.add(b.label)
    return core


class Bag(dag.Node):
  def __init__(self, sack, index, url):
    super(Bag, self).__init__(index)
    self._sack = sack
    self._url = url
    self._label = self._MakeShortname(url)
    # Maps a ResourceGraph to its Nodes contained in this Bag.
    self._graphs = defaultdict(set)
    # Maps each successor bag to the set of (graph, node, graph-successor)
    # tuples that generated it.
    self._successor_sources = defaultdict(set)
    # Maps each successor bag to a set of edge costs. This is just used to
    # track min and max; if we want more statistics we'd have to count the
    # costs with multiplicity.
    self._successor_edge_costs = defaultdict(set)

    # Miscellaneous counts and costs used in display.
    self._total_costs = []
    self._relative_costs = []
    self._num_critical = 0

    # See MarkOccurrence and GetOccurrence, below. This maps an occurrence
    # tag to a list of nodes matching the occurrence.
    self._occurence_matches = {}
    # Number of nodes seen for each occurrence.
    self._occurence_count = {}

  @property
  def url(self):
    return self._url

  @property
  def label(self):
    return self._label

  @property
  def graphs(self):
    return self._graphs

  @property
  def successor_sources(self):
    return self._successor_sources

  @property
  def successor_edge_costs(self):
    return self._successor_edge_costs

  @property
  def total_costs(self):
    return self._total_costs

  @property
  def relative_costs(self):
    return self._relative_costs

  @property
  def num_critical(self):
    return self._num_critical

  @property
  def num_nodes(self):
    return len(self._total_costs)

  def MarkCritical(self):
    self._num_critical += 1

  def AddNode(self, graph, node):
    if node in self._graphs[graph]:
      return  # Already added.
    graph_info = self._sack.graph_info[graph]
    self._graphs[graph].add(node)
    node_total_cost = graph_info.total_costs[node.Index()]
    self._total_costs.append(node_total_cost)
    self._relative_costs.append(
        float(node_total_cost) / graph_info.cost)
    for s in node.Node().Successors():
      if not graph._node_filter(s):
        continue
      node_info = graph.NodeInfo(s)
      successor_bag = self._sack.AddNode(graph, node_info)
      self.AddSuccessor(successor_bag)
      self._successor_sources[successor_bag].add((graph, node, s))
      self._successor_edge_costs[successor_bag].add(graph.EdgeCost(node, s))

  def MarkOccurrence(self, tag, filter_from_graph):
    """Mark occurrences for nodes in this bag according to graph_filters.

    Results can be querried by GetOccurrence().

    Args:
      tag: a label for this set of occurrences.
      filter_from_graph: a function graph -> node filter, where node filter
        takes a node to a boolean.
    """
    self._occurence_matches[tag] = 0
    self._occurence_count[tag] = 0
    for graph, nodes in self.graphs.iteritems():
      for n in nodes:
        self._occurence_count[tag] += 1
        if filter_from_graph(graph)(n):
          self._occurence_matches[tag] += 1

  def GetOccurrence(self, tag):
    """Retrieve the occurrence fraction of a tag.

    Args:
      tag: the tag under which the occurrence was counted. This must have been
        previously added at least once via AddOccurrence.

    Returns:
      A fraction occurrence matches / occurrence node count.
    """
    assert self._occurence_count[tag] > 0
    return float(self._occurence_matches[tag]) / self._occurence_count[tag]

  @classmethod
  def _MakeShortname(cls, url):
    parsed = urlparse.urlparse(url)
    if parsed.scheme == 'data':
      if ';' in parsed.path:
        kind, _ = parsed.path.split(';', 1)
      else:
        kind, _ = parsed.path.split(',', 1)
      return 'data:' + kind
    path = parsed.path[:10]
    hostname = parsed.hostname if parsed.hostname else '?.?.?'
    return hostname + '/' + path
