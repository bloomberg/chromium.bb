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
    hw_test_configs=[],
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

  hw_test_configs = sorted(hw_test_configs, key=lambda t: t.suite)
  hw_test_cfg = config_pb.HwTestCfg(hw_test=hw_test_configs)

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
      hw_test_cfg=hw_test_cfg)
  graph.add_node(
      graph.key(test_reqs_kind, target_name),
      props={'target_test_requirements': testing_reqs})
  graph.add_edge(root_key, graph.key(test_reqs_kind, target_name))


def _hw_test_config(
    suite_name,
    blocking,
    minimum_duts,
    timeout_sec=5400,
    pool='cq',
    async=False,
    warn_only=False,
    critical=False,
    priority='CQ',
    file_bugs=False,
    retry=True,
    max_retries=5,
    suite_min_duts=0,
    offload_failures_only=False):
  """Creates a HwTest with the supplied settings.

  See the proto for full descriptions of what each arg is.

  Args:
    suite_name: string
    blocking: bool
    minimum_duts: int
    timeout_sec: int
    pool: string
    async: bool
    warn_only: bool
    critical: bool
    priority: bool
    file_bugs: bool
    retry: bool
    max_retries: int
    suite_min_duts: int
    offload_failures_only: bool

  Returns:
    config_pb.HwTest
  """
  return config_pb.HwTestCfg.HwTest(
    suite=suite_name,
    blocking=blocking,
    minimum_duts=minimum_duts,
    timeout_sec=timeout_sec,
    pool=pool,
    async=async,
    warn_only=warn_only,
    critical=critical,
    priority=priority,
    file_bugs=file_bugs,
    retry=retry,
    max_retries=max_retries,
    suite_min_duts=suite_min_duts,
    offload_failures_only=offload_failures_only)


def standard_bvt_inline():
  """Creates a bvt-inline HwTest with default settings.

  Returns:
    config_pb.HwTest
  """
  return _hw_test_config(
    suite_name='bvt-inline',
    blocking=False,
    minimum_duts=4)


def standard_bvt_cq():
  """Creates a bvt-cq HwTest with default settings.

  Returns:
    config_pb.HwTest
  """
  return _hw_test_config(
    suite_name='bvt-cq',
    blocking=False,
    minimum_duts=4)


def standard_bvt_tast_cq():
  """Creates a bvt-tast-cq HwTest with default settings.

  Returns:
    config_pb.HwTest
  """
  return _hw_test_config(
    suite_name='bvt-tast-cq',
    blocking=False,
    minimum_duts=1)


def standard_bvt_arc():
  """Creates a bvt-arc HwTest with default settings.

  Returns:
    config_pb.HwTest
  """
  return _hw_test_config(
    suite_name='bvt-arc',
    blocking=False,
    minimum_duts=4)



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
