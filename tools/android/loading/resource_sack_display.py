# Copyright 2016 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Utilities for displaying a ResourceSack.

When run standalone, takes traces on the command line and produces a dot file to
stdout.
"""


def ToDot(sack, output, prune=-1, long_edge_msec=2000):
  """Output as a dot file.

  Args:
    sack: (ResourceSack) the sack to convert to dot.
    output: a file-like output stream.
    prune: if positive, prune & coalesce nodes under the specified threshold
      of repeated views, as fraction node views / total graphs. All pruned
      nodes are represented by a single node, and an edge is connected only if
      the view count is greater than 1.
    long_edge_msec: if positive, the definition of a long edge. Long edges are
      distinguished in graph.
  """
  output.write("""digraph dependencies {
  rankdir = LR;
  """)

  pruned = set()
  num_graphs = len(sack.graph_info)
  for bag in sack.bags:
    if prune > 0 and float(len(bag.graphs)) / num_graphs < prune:
      pruned.add(bag)
      continue
    output.write('%d [label="%s (%d)\n(%d, %d)\n(%.2f, %.2f)" shape=%s; '
                 'style=filled; fillcolor=%s];\n' % (
        bag.Index(), bag.label, len(bag.graphs),
        min(bag.total_costs), max(bag.total_costs),
        min(bag.relative_costs), max(bag.relative_costs),
        _CriticalToShape(bag),
        _AmountToNodeColor(len(bag.graphs), num_graphs)))

  if pruned:
    pruned_index = num_graphs
    output.write('%d [label="Pruned at %.0f%%\n(%d)"; '
                 'shape=polygon; style=dotted];\n' %
                 (pruned_index, 100 * prune, len(pruned)))

  for bag in sack.bags:
    if bag in pruned:
      for succ in bag.Successors():
        if succ not in pruned:
          output.write('%d -> %d [style=dashed];\n' % (
              pruned_index, succ.Index()))
    for succ in bag.Successors():
      if succ in pruned:
        if len(bag.successor_sources[succ]) > 1:
          output.write('%d -> %d [label="%d"; style=dashed];\n' % (
              bag.Index(), pruned_index, len(bag.successor_sources[succ])))
      else:
        num_succ = len(bag.successor_sources[succ])
        num_long = 0
        for graph, source, target in bag.successor_sources[succ]:
          if graph.EdgeCost(source, target) > long_edge_msec:
            num_long += 1
        if num_long > 0:
          long_frac = float(num_long) / num_succ
          long_edge_style = '; penwidth=%f' % (2 + 6.0 * long_frac)
          if long_frac < 0.75:
            long_edge_style += '; style=dashed'
        else:
          long_edge_style = ''
        min_edge = min(bag.successor_edge_costs[succ])
        max_edge = max(bag.successor_edge_costs[succ])
        output.write('%d -> %d [label="%d\n(%f,%f)"; color=%s %s];\n' % (
            bag.Index(), succ.Index(), num_succ, min_edge, max_edge,
            _AmountToEdgeColor(num_succ, len(bag.graphs)),
            long_edge_style))

  output.write('}')


def _CriticalToShape(bag):
  frac = float(bag.num_critical) / bag.num_nodes
  if frac < 0.4:
    return 'oval'
  elif frac < 0.7:
    return 'polygon'
  elif frac < 0.9:
    return 'trapezium'
  return 'box'


def _AmountToNodeColor(numer, denom):
  if denom <= 0:
    return 'grey72'
  ratio = 1.0 * numer / denom
  if ratio < .3:
    return 'white'
  elif ratio < .6:
    return 'yellow'
  elif ratio < .8:
    return 'orange'
  return 'green'


def _AmountToEdgeColor(numer, denom):
  color = _AmountToNodeColor(numer, denom)
  if color == 'white' or color == 'grey72':
    return 'black'
  return color


def _Main():
  import json
  import logging
  import sys

  import loading_model
  import loading_trace
  import resource_sack

  sack = resource_sack.GraphSack()
  for fname in sys.argv[1:]:
    trace = loading_trace.LoadingTrace.FromJsonDict(
      json.load(open(fname)))
    logging.info('Making graph from %s', fname)
    model = loading_model.ResourceGraph(trace, content_lens=None)
    sack.ConsumeGraph(model)
    logging.info('Finished %s', fname)
  ToDot(sack, sys.stdout, prune=.1)


if __name__ == '__main__':
  _Main()
