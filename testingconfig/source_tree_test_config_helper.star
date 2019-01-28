# Copyright 2019 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

load('@stdlib//internal/graph.star', 'graph')
load(
    "@proto//external/crostesting/source_tree_test_config.proto",
    source_tree_test_config_pb="crostesting")

source_tree_kind = 'source_tree'
# Set up the root node in the graph.
root_key = graph.key(source_tree_kind, '...')
graph.add_node(root_key)

def source_tree_test_config(
    source_trees,
    disable_hw_tests=False,
    disable_image_tests=False,
    disable_vm_tests=False):
  """Create SourceTreeTestRestriction and bind it in the Starlark graph.

  Args:
    source_trees: list(string) of source tree paths
    disable_hw_tests: boolean, whether to disable hardware tests
    disable_image_tests: boolean, whether to disable image tests
    disable_vm_tests: boolean, whether to disable virtual machine tests
  """
  for source_tree_name in source_trees:
    source_tree_test_restriction = (
        source_tree_test_config_pb.SourceTreeTestRestriction(
            source_tree = source_tree_test_config_pb.SourceTree(
                path=source_tree_name)))

    test_restriction = source_tree_test_config_pb.TestRestriction(
        disable_hw_tests=disable_hw_tests,
        disable_image_tests=disable_image_tests,
        disable_vm_tests=disable_vm_tests)
    source_tree_test_restriction.test_restriction = test_restriction

    graph.add_node(
        graph.key(source_tree_kind, source_tree_name),
        props={'source_tree_test_restriction': source_tree_test_restriction})
    graph.add_edge(root_key, graph.key(source_tree_kind, source_tree_name))


def gen(ctx):
  """Called by lucicfg to run Starlark and generate the output config."""
  # Collect the restrictions and sort them on source tree path.
  source_tree_test_restrictions = (
      [n.props.source_tree_test_restriction
       for n in graph.children(root_key, source_tree_kind)])
  sorted(source_tree_test_restrictions, key=lambda s: s.source_tree.path)
  repo_test_config = source_tree_test_config_pb.SourceTreeTestCfg(
      source_tree_test_restriction=source_tree_test_restrictions
  )
  ctx.config_set['source_tree_test_config.cfg'] = (
      proto.to_jsonpb(repo_test_config))

core.generator(impl = gen)
