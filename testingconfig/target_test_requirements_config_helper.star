# Copyright 2019 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

load('@stdlib//internal/graph.star', 'graph')
load(
    '@proto//external/crostesting/target_test_requirements_config.proto',
    config_pb='crostesting')

test_reqs_kind = 'target_test_reqs'
# Set up the root node in the graph.
root_key = graph.key(test_reqs_kind, '...')


def set_up_graph():
  """Call this once to set up the Starlark graph root node."""
  graph.add_node(root_key)


def target_test_requirements(
    reference_design=None,
    build_target=None,
    test_suites=[]):
  """Create a PerTargetTestRestriction and binds it in the Starlark graph.

  Exactly one of reference_design or build_target must be selected. The former
  allows testing to be performed on any device in that reference design's
  family. The latter will force testing to be performed on that specific board.

  Args:
    reference_design: string, a mosys platform family, e.g. Google_Reef
    build_target: string, a particular CrOS build target, e.g. kevin
    test_suites: list(string), the test suites to run
  """

  if bool(reference_design) == bool(build_target):
    fail('Expected exactly one of reference_design and build_target to be set. '
         + 'Instead, got %s and %s'%(reference_design, build_target))

  sorted(test_suites)
  test_suite_pbs = (
      [config_pb.TestSuite(test_suite_name=test_suite)
       for test_suite in test_suites])

  if reference_design:
    target_name = reference_design
    build_criteria = config_pb.BuildCriteria(
         reference_design=reference_design)
  else:
    target_name = build_target
    build_criteria = config_pb.BuildCriteria(
         build_target=build_target)

  testing_reqs = config_pb.PerTargetTestRequirements(
      build_criteria=build_criteria,
      test_suite=test_suite_pbs)
  graph.add_node(
      graph.key(test_reqs_kind, target_name),
      props={'target_test_requirements': testing_reqs})
  graph.add_edge(root_key, graph.key(test_reqs_kind, target_name))


def gen(ctx):
  """Called by lucicfg to run Starlark and generate the output config."""
  target_test_requirements_by_name = {
      n.key.id:n.props.target_test_requirements
          for n in graph.children(root_key, test_reqs_kind)
  }
  target_test_requirements = [
      target_test_requirements_by_name[key]
          for key in sorted(target_test_requirements_by_name.keys())
  ]
  target_test_requirements_pb = config_pb.TargetTestRequirementsCfg(
      per_target_test_requirements = target_test_requirements,
  )
  ctx.config_set['target_test_requirements.cfg'] = (
      proto.to_jsonpb(target_test_requirements_pb))

lucicfg.generator(impl = gen)
