#! /usr/bin/python
# Copyright 2015 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Creates a Graphviz file visualizing the resource dependencies from a JSON
file dumped by log_requests.py.
"""

import collections
import sys
import urlparse

import log_parser
from log_parser import Resource


def _BuildResourceDependencyGraph(requests):
  """Builds the graph of resource dependencies.

  Args:
    requests: [RequestData, ...]

  Returns:
    A tuple ([Resource], [(resource1, resource2, reason), ...])
  """
  resources = log_parser.GetResources(requests)
  resources_from_url = {resource.url: resource for resource in resources}
  requests_by_completion = log_parser.SortedByCompletion(requests)
  deps = []
  for r in requests:
    resource = Resource.FromRequest(r)
    initiator = r.initiator
    initiator_type = initiator['type']
    dep = None
    if initiator_type == 'parser':
      url = initiator['url']
      blocking_resource = resources_from_url.get(url, None)
      if blocking_resource is None:
        continue
      dep = (blocking_resource, resource, 'parser')
    elif initiator_type == 'script' and 'stackTrace' in initiator:
      for frame in initiator['stackTrace']:
        url = frame['url']
        blocking_resource = resources_from_url.get(url, None)
        if blocking_resource is None:
          continue
        dep = (blocking_resource, resource, 'stack')
        break
    else:
      # When the initiator is a script without a stackTrace, infer that it comes
      # from the most recent script from the same hostname.
      # TLD+1 might be better, but finding what is a TLD requires a database.
      request_hostname = urlparse.urlparse(r.url).hostname
      sorted_script_requests_from_hostname = [
          r for r in requests_by_completion
          if (resource.GetContentType() in ('script', 'html', 'json')
              and urlparse.urlparse(r.url).hostname == request_hostname)]
      most_recent = None
      # Linear search is bad, but this shouldn't matter here.
      for request in sorted_script_requests_from_hostname:
        if request.timestamp < r.timing.requestTime:
          most_recent = request
        else:
          break
      if most_recent is not None:
        blocking = resources_from_url.get(most_recent.url, None)
        if blocking is not None:
          dep = (blocking, resource, 'script_inferred')
    if dep is not None:
      deps.append(dep)
  return (resources, deps)


def PrefetchableResources(requests):
  """Returns a list of resources that are discoverable without JS.

  Args:
    requests: List of requests.

  Returns:
    List of discoverable resources, with their initial request.
  """
  resource_to_request = log_parser.ResourceToRequestMap(requests)
  (_, all_deps) = _BuildResourceDependencyGraph(requests)
  # Only keep "parser" arcs
  deps = [(first, second) for (first, second, reason) in all_deps
          if reason == 'parser']
  deps_per_resource = collections.defaultdict(list)
  for (first, second) in deps:
    deps_per_resource[first].append(second)
  result = []
  visited = set()
  to_visit = [deps[0][0]]
  while len(to_visit) != 0:
    r = to_visit.pop()
    visited.add(r)
    to_visit += deps_per_resource[r]
    result.append(resource_to_request[r])
  return result


_CONTENT_TYPE_TO_COLOR = {'html': 'red', 'css': 'green', 'script': 'blue',
                          'json': 'purple', 'gif_image': 'grey',
                          'image': 'orange', 'other': 'white'}


def _ResourceGraphvizNode(resource, request, resource_to_index):
  """Returns the node description for a given resource.

  Args:
    resource: Resource.
    request: RequestData associated with the resource.
    resource_to_index: {Resource: int}.

  Returns:
    A string describing the resource in graphviz format.
    The resource is color-coded according to its content type, and its shape is
    oval if its max-age is less than 300s (or if it's not cacheable).
  """
  color = _CONTENT_TYPE_TO_COLOR[resource.GetContentType()]
  max_age = log_parser.MaxAge(request)
  shape = 'polygon' if max_age > 300 else 'oval'
  return ('%d [label = "%s"; style = "filled"; fillcolor = %s; shape = %s];\n'
          % (resource_to_index[resource], resource.GetShortName(), color,
             shape))


def _GraphvizFileFromDeps(resources, requests, deps, output_filename):
  """Writes a graphviz file from a set of resource dependencies.

  Args:
    resources: [Resource, ...]
    requests: list of requests
    deps: [(resource1, resource2, reason), ...]
    output_filename: file to write the graph to.
  """
  with open(output_filename, 'w') as f:
    f.write("""digraph dependencies {
    rankdir = LR;
    """)
    resource_to_request = log_parser.ResourceToRequestMap(requests)
    resource_to_index = {r: i for (i, r) in enumerate(resources)}
    resources_with_edges = set()
    for (first, second, reason) in deps:
      resources_with_edges.add(first)
      resources_with_edges.add(second)
    if len(resources_with_edges) != len(resources):
      f.write("""subgraph cluster_orphans {
  color=black;
  label="Orphans";
""")
      for resource in resources:
        if resource not in resources_with_edges:
          request = resource_to_request[resource]
          f.write(_ResourceGraphvizNode(resource, request, resource_to_index))
      f.write('}\n')

    f.write("""subgraph cluster_nodes {
  color=invis;
""")
    for resource in resources:
      request = resource_to_request[resource]
      print resource.url
      if resource in resources_with_edges:
        f.write(_ResourceGraphvizNode(resource, request, resource_to_index))
    for (first, second, reason) in deps:
      arrow = ''
      if reason == 'parser':
        arrow = '[color = red]'
      elif reason == 'stack':
        arrow = '[color = blue]'
      elif reason == 'script_inferred':
        arrow = '[color = blue; style=dotted]'
      f.write('%d -> %d %s;\n' % (
          resource_to_index[first], resource_to_index[second], arrow))
    f.write('}\n}\n')


def main():
  filename = sys.argv[1]
  requests = log_parser.ParseJsonFile(filename)
  requests = log_parser.FilterRequests(requests)
  (resources, deps) = _BuildResourceDependencyGraph(requests)
  _GraphvizFileFromDeps(resources, requests, deps, filename + '.dot')


if __name__ == '__main__':
  main()
