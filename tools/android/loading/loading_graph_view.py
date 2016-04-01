# Copyright 2016 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Views a trace as an annotated request dependency graph."""

import dependency_graph


class RequestNode(dependency_graph.RequestNode):
  """Represents a request in the graph.

  is_ad and is_tracking are set according to the ContentClassificationLens
  passed to LoadingGraphView.
  """
  def __init__(self, request):
    super(RequestNode, self).__init__(request)
    self.is_ad = False
    self.is_tracking = False


class Edge(dependency_graph.Edge):
  """Represents a dependency between two nodes.

  activity is set according to the ActivityLens passed to LoadingGraphView.
  """
  def __init__(self, from_node, to_node, reason):
    super(Edge, self).__init__(from_node, to_node, reason)
    self.activity = {}


class LoadingGraphView(object):
  """Represents a trace as a dependency graph. The graph is annotated using
     optional lenses passed to it.
  """
  def __init__(self, trace, dependencies_lens, content_lens=None,
               frame_lens=None, activity=None):
    """Initalizes a LoadingGraphView instance.

    Args:
      trace: (LoadingTrace) a loading trace.
      dependencies_lens: (RequestDependencyLens)
      content_lens: (ContentClassificationLens)
      frame_lens: (FrameLoadLens)
      activity: (ActivityLens)
    """
    self._requests = trace.request_track.GetEvents()
    self._deps_lens = dependencies_lens
    self._content_lens = content_lens
    self._frame_lens = frame_lens
    self._activity_lens = activity
    self._graph = None
    self._BuildGraph()

  def RemoveAds(self):
    """Updates the graph to remove the Ads.

    Nodes that are only reachable through ad nodes are excluded as well.
    """
    roots = self._graph.graph.RootNodes()
    self._requests = [n.request for n in self._graph.graph.ReachableNodes(
        roots, should_stop=lambda n: n.is_ad or n.is_tracking)]
    self._BuildGraph()

  @property
  def deps_graph(self):
    return self._graph

  def _BuildGraph(self):
    self._graph = dependency_graph.RequestDependencyGraph(
        self._requests, self._deps_lens, RequestNode, Edge)
    self._AnnotateNodes()
    self._AnnotateEdges()

  def _AnnotateNodes(self):
    if self._content_lens is None:
      return
    for node in self._graph.graph.Nodes():
      node.is_ad = self._content_lens.IsAdRequest(node.request)
      node.is_tracking = self._content_lens.IsTrackingRequest(node.request)

  def _AnnotateEdges(self):
    if self._activity_lens is None:
      return
    for edge in self._graph.graph.Edges():
      dep = (edge.from_node.request, edge.to_node.request, edge.reason)
      activity = self._activity_lens.BreakdownEdgeActivityByInitiator(dep)
      edge.activity = activity
