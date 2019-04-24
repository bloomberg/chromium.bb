# -*- coding: utf-8 -*-
# Copyright 2019 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Build graph dependency creation service.

This service handles the creation of the portage build dependency graphs and the
graphs mapping from portage packages to the dependency source.
"""

from __future__ import print_function

from chromite.api.gen import depgraph_pb2
from chromite.lib import portage_util
from chromite.service import dependency

from google.protobuf import json_format


def CreateDepGraphProtoFromJsonMap(json_map):
  """Return the depgraph proto message from package deps json map.

  Args:
    json_map: the json object that stores the portage package. This is
      generated from chromite.lib.service.dependency.GetBuildDependency()
  """
  graph = depgraph_pb2.DepGraph()
  graph.build_target.name = json_map['target_board']

  for data in json_map['package_deps'].itervalues():
    package_dep_info = graph.package_deps.add()
    package_info = package_dep_info.package_info
    package_info.package_name = data['name']
    package_info.category = data['category']
    package_info.version = data['version']
    for dep in data['deps']:
      cpv = portage_util.SplitCPV(dep, strict=False)
      dep_package = package_dep_info.dependency_packages.add()
      dep_package.package_name = cpv.package
      dep_package.category = cpv.category
      if cpv.version:
        dep_package.version = cpv.version

    package_CPV = '%s/%s-%s' % (
        package_info.category, package_info.package_name, package_info.version)
    for path in json_map['source_path_mapping'][package_CPV]:
      source_path = package_dep_info.dependency_source_paths.add()
      source_path.path = path

  return graph

def GetBuildDependencyGraph(input_proto, output_proto):
  """Create the build dependency graph.

  Args:
    input_proto (GetBuildDependencyGraphRequest): The input arguments message.
    output_proto (GetBuildDependencyGraphResponse): The empty output message.
  """
  board = input_proto.build_target.name
  output_path = input_proto.output_path

  assert board, 'Missing build target name'
  assert output_path, 'Missing output file'


  json_map = dependency.GetBuildDependency(board)
  graph = CreateDepGraphProtoFromJsonMap(json_map)

  with open(output_path, 'w') as f:
    f.write(
        json_format.MessageToJson(graph, including_default_value_fields=True))

  output_proto.build_dependency_graph_file = output_path
