# Copyright 2016 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Visualize a loading_model.ResourceGraph."""

import dag
import itertools

import loading_model


class GraphVisualization(object):
  """Manipulate visual representations of a resource graph.

  The output will change as the ResourceGraph is changed, for example by setting
  filters.

  Currently only DOT output is supported.
  """
  _LONG_EDGE_THRESHOLD_MS = 2000  # Time in milliseconds.

  _CONTENT_KIND_TO_COLOR = {
      'application':     'blue',      # Scripts.
      'font':            'grey70',
      'image':           'orange',    # This probably catches gifs?
      'video':           'hotpink1',
      'audio':           'hotpink2',
      }

  _CONTENT_TYPE_TO_COLOR = {
      'html':            'red',
      'css':             'green',
      'script':          'blue',
      'javascript':      'blue',
      'json':            'purple',
      'gif':             'grey',
      'image':           'orange',
      'jpeg':            'orange',
      'png':             'orange',
      'plain':           'brown3',
      'octet-stream':    'brown3',
      'other':           'white',
      'synthetic':       'yellow',
      }

  _EDGE_KIND_TO_COLOR = {
    'redirect': 'black',
    'parser': 'red',
    'script': 'blue',
    'script_inferred': 'purple',
    'after-load': 'forestgreen',
    'before-load': 'forestgreen',
  }

  _ACTIVITY_TYPE_LABEL = (
      ('script', 'S'), ('parsing', 'P'), ('other_url', 'O'),
      ('unknown_url', 'U'))

  def __init__(self, graph):
    """Initialize.

    Args:
      graph: (loading_model.ResourceGraph) the graph to visualize.
    """
    self._graph = graph
    self._global_start = None

  def OutputDot(self, output):
    """Output DOT (graphviz) representation.

    Args:
      output: a file-like output stream to receive the dot file.
    """
    sorted_nodes = [n for n in self._graph.Nodes(sort=True)]
    self._global_start = min([n.StartTime() for n in sorted_nodes])
    visited_nodes = set([n for n in sorted_nodes])

    output.write("""digraph dependencies {
    rankdir = LR;
    """)

    orphans = set()
    for n in sorted_nodes:
      for s in itertools.chain(n.Node().Successors(),
                               n.Node().Predecessors()):
        if s in visited_nodes:
          break
      else:
        orphans.add(n)
    if orphans:
      output.write("""subgraph cluster_orphans {
                        color=black;
                        label="Orphans";
                   """)
      for n in orphans:
        # Ignore synthetic nodes for orphan display.
        if not self._graph.NodeInfo(n).Request():
          continue
        output.write(self.DotNode(n))
      output.write('}\n')

    output.write("""subgraph cluster_nodes {
                      color=invis;
                 """)

    for n in sorted_nodes:
      if n in orphans:
        continue
      output.write(self.DotNode(n))

    for n in visited_nodes:
      for s in n.Node().Successors():
        if s not in visited_nodes:
          continue
        style = 'color = orange'
        label = '%.02f' % self._graph.EdgeCost(n, s)
        annotations = self._graph.EdgeAnnotations(n, s)
        edge_kind = annotations.get(
            loading_model.ResourceGraph.EDGE_KIND_KEY, None)
        assert ((edge_kind is None)
                or (edge_kind in loading_model.ResourceGraph.EDGE_KINDS))
        style = 'color = %s' % self._EDGE_KIND_TO_COLOR[edge_kind]
        if edge_kind == 'timing':
          style += '; style=dashed'
        if self._graph.EdgeCost(n, s) > self._LONG_EDGE_THRESHOLD_MS:
          style += '; penwidth=5; weight=2'

        label = '%.02f' % self._graph.EdgeCost(n, s)
        if 'activity' in annotations:
          activity = annotations['activity']
          separator = ' - '
          for activity_type, activity_label in self._ACTIVITY_TYPE_LABEL:
            label += '%s%s:%.02f ' % (
                separator, activity_label, activity[activity_type])
            separator = ' '
        arrow = '[%s; label="%s"]' % (style, label)
        output.write('%d -> %d %s;\n' % (n.Index(), s.Index(), arrow))
    output.write('}\n')

    output.write('}\n')

  def _ContentTypeToColor(self, content_type):
    if not content_type:
      type_str = 'other'
    elif '/' in content_type:
      kind, type_str = content_type.split('/', 1)
      if kind in self._CONTENT_KIND_TO_COLOR:
        return self._CONTENT_KIND_TO_COLOR[kind]
    else:
      type_str = content_type
    return self._CONTENT_TYPE_TO_COLOR[type_str]

  def DotNode(self, node):
    """Returns a graphviz node description for a given node.

    Args:
      node: a dag.Node or ResourceGraph node info.

    Returns:
      A string describing the resource in graphviz format.
      The resource is color-coded according to its content type, and its shape
      is oval if its max-age is less than 300s (or if it's not cacheable).
    """
    if type(node) is dag.Node:
      node = self._graph.NodeInfo(node)
    color = self._ContentTypeToColor(node.ContentType())
    if node.Request():
      max_age = node.Request().MaxAge()
      shape = 'polygon' if max_age > 300 else 'oval'
    else:
      shape = 'doubleoctagon'
    styles = ['filled']
    if node.IsAd() or node.IsTracking():
      styles += ['bold', 'diagonals']
    return ('%d [label = "%s\\n%.2f->%.2f (%.2f)"; style = "%s"; '
            'fillcolor = %s; shape = %s];\n'
            % (node.Index(), node.ShortName(),
               node.StartTime() - self._global_start,
               node.EndTime() - self._global_start,
               node.EndTime() - node.StartTime(),
               ','.join(styles), color, shape))
