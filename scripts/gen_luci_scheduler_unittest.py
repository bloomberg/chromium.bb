# -*- coding: utf-8 -*-
# Copyright 2018 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Test gen_luci_scheduler."""
from __future__ import print_function

from chromite.config import chromeos_config
from chromite.lib import cros_test_lib
from chromite.lib import config_lib
from chromite.lib import config_lib_unittest
from chromite.scripts import gen_luci_scheduler

# It's reasonable for unittests to look at internals.
# pylint: disable=protected-access


class GenLuciSchedulerTest(cros_test_lib.MockTestCase):
  """Tests for cbuildbot_launch script."""
  def testSanityAgainstProd(self):
    """Test we can generate a luci scheduler config with live data."""
    # If it runs without crashing, we pass.
    gen_luci_scheduler.genLuciSchedulerConfig(
        config_lib.GetConfig(), chromeos_config.BranchScheduleConfig())

  def testGenSchedulerJob(self):
    """Test the job creation helper."""
    build_config = config_lib_unittest.MockBuildConfig().apply(
        schedule='funky schedule'
    )

    expected = '''
job {
  id: "amd64-generic-paladin"
  acl_sets: "default"
  schedule: "funky schedule"
  buildbucket: {
    server: "cr-buildbucket.appspot.com"
    bucket: "luci.chromeos.general"
    builder: "Prod"
    tags: "cbb_branch:master"
    tags: "cbb_config:amd64-generic-paladin"
    tags: "cbb_display_label:MockLabel"
    properties: "cbb_branch:master"
    properties: "cbb_config:amd64-generic-paladin"
    properties: "cbb_display_label:MockLabel"
    properties: "cbb_extra_args:[\\"--buildbot\\"]"
  }
}
'''

    result = gen_luci_scheduler.genSchedulerJob(build_config)
    self.assertEqual(result, expected)

  def testGenSchedulerTriggerSimple(self):
    """Test the trigger creation helper."""
    trigger_name = 'simple'
    repo = 'url://repo'
    refs = ['refs/path']
    builds = ['test_build']

    expected = '''
trigger {
  id: "simple"
  acl_sets: "default"
  schedule: "with 5m interval"
  gitiles: {
    repo: "url://repo"
    refs: "refs/path"
  }
  triggers: "test_build"
}
'''

    result = gen_luci_scheduler.genSchedulerTrigger(
        trigger_name, repo, refs, builds)

    self.assertEqual(result, expected)

  def testGenSchedulerTriggerComplex(self):
    """Test the trigger creation helper."""
    trigger_name = 'complex'
    repo = 'url://repo'
    refs = ['refs/path', 'refs/other_path']
    builds = ['test_build_a', 'test_build_b']

    expected = '''
trigger {
  id: "complex"
  acl_sets: "default"
  schedule: "with 5m interval"
  gitiles: {
    repo: "url://repo"
    refs: "refs/path"
    refs: "refs/other_path"
  }
  triggers: "test_build_a"
  triggers: "test_build_b"
}
'''

    result = gen_luci_scheduler.genSchedulerTrigger(
        trigger_name, repo, refs, builds)

    self.assertEqual(result, expected)


  def testGenSchedulerBranched(self):
    """Test the job creation helper."""
    build_config = config_lib_unittest.MockBuildConfig().apply(
        schedule_branch='mock_branch',
        schedule='funky schedule',
    )

    expected = '''
job {
  id: "mock_branch-amd64-generic-paladin"
  acl_sets: "default"
  schedule: "funky schedule"
  buildbucket: {
    server: "cr-buildbucket.appspot.com"
    bucket: "luci.chromeos.general"
    builder: "Prod"
    tags: "cbb_branch:mock_branch"
    tags: "cbb_config:amd64-generic-paladin"
    tags: "cbb_display_label:MockLabel"
    properties: "cbb_branch:mock_branch"
    properties: "cbb_config:amd64-generic-paladin"
    properties: "cbb_display_label:MockLabel"
    properties: "cbb_extra_args:[\\"--buildbot\\"]"
  }
}
'''

    result = gen_luci_scheduler.genSchedulerJob(build_config)
    self.assertEqual(result, expected)

  def testGenSchedulerWorkspaceBranch(self):
    """Test the job creation helper."""
    build_config = config_lib_unittest.MockBuildConfig().apply(
        workspace_branch='work_branch',
        schedule='funky schedule',
    )

    expected = '''
job {
  id: "amd64-generic-paladin"
  acl_sets: "default"
  schedule: "funky schedule"
  buildbucket: {
    server: "cr-buildbucket.appspot.com"
    bucket: "luci.chromeos.general"
    builder: "Prod"
    tags: "cbb_branch:master"
    tags: "cbb_config:amd64-generic-paladin"
    tags: "cbb_display_label:MockLabel"
    tags: "cbb_workspace_branch:work_branch"
    properties: "cbb_branch:master"
    properties: "cbb_config:amd64-generic-paladin"
    properties: "cbb_display_label:MockLabel"
    properties: "cbb_workspace_branch:work_branch"
    properties: "cbb_extra_args:[\\"--buildbot\\"]"
  }
}
'''

    result = gen_luci_scheduler.genSchedulerJob(build_config)
    self.assertEqual(result, expected)

  def testGenLuciSchedulerConfig(self):
    """Test a full LUCI Scheduler config file."""
    site_config = config_lib.SiteConfig()

    site_config.Add(
        'not_scheduled',
        luci_builder='ProdBuilder',
        display_label='MockLabel',
    )

    site_config.Add(
        'build_prod',
        luci_builder='ProdBuilder',
        display_label='MockLabel',
        schedule='run once in a while',
    )

    site_config.Add(
        'build_tester',
        luci_builder='TestBuilder',
        display_label='TestLabel',
        schedule='run daily',
    )

    site_config.Add(
        'build_triggered_a',
        luci_builder='ProdBuilder',
        display_label='MockLabel',
        schedule='triggered',
        triggered_gitiles=[[
            'gitiles_url_a',
            ['ref_a', 'ref_b'],
        ], [
            'gitiles_url_b',
            ['ref_c'],
        ]],
    )

    site_config.Add(
        'build_triggered_b',
        luci_builder='ProdBuilder',
        display_label='MockLabel',
        schedule='triggered',
        triggered_gitiles=[[
            'gitiles_url_b',
            ['ref_c'],
        ]],
    )

    default_config = config_lib.GetConfig().GetDefault()

    branch_configs = [
        default_config.derive(
            name='branch_tester',
            luci_builder='TestBuilder',
            display_label='TestLabel',
            schedule='run daily',
            schedule_branch='test-branch',
        ),
    ]


    expected = '''# Defines buckets on luci-scheduler.appspot.com.
#
# For schema of this file and documentation see ProjectConfig message in
# https://github.com/luci/luci-go/blob/master/scheduler/appengine/messages/config.proto

# Generated with chromite/scripts/gen_luci_scheduler

acl_sets {
  name: "default"
  acls {
    role: READER
    granted_to: "group:googlers"
  }
  acls {
    role: OWNER
    granted_to: "group:project-chromeos-admins"
  }
}

trigger {
  id: "trigger_0"
  acl_sets: "default"
  schedule: "with 5m interval"
  gitiles: {
    repo: "gitiles_url_a"
    refs: "ref_a"
    refs: "ref_b"
  }
  triggers: "build_triggered_a"
}

trigger {
  id: "trigger_1"
  acl_sets: "default"
  schedule: "with 5m interval"
  gitiles: {
    repo: "gitiles_url_b"
    refs: "ref_c"
  }
  triggers: "build_triggered_a"
  triggers: "build_triggered_b"
}

job {
  id: "build_prod"
  acl_sets: "default"
  schedule: "run once in a while"
  buildbucket: {
    server: "cr-buildbucket.appspot.com"
    bucket: "luci.chromeos.general"
    builder: "ProdBuilder"
    tags: "cbb_branch:master"
    tags: "cbb_config:build_prod"
    tags: "cbb_display_label:MockLabel"
    properties: "cbb_branch:master"
    properties: "cbb_config:build_prod"
    properties: "cbb_display_label:MockLabel"
    properties: "cbb_extra_args:[\\"--buildbot\\"]"
  }
}

job {
  id: "build_tester"
  acl_sets: "default"
  schedule: "run daily"
  buildbucket: {
    server: "cr-buildbucket.appspot.com"
    bucket: "luci.chromeos.general"
    builder: "TestBuilder"
    tags: "cbb_branch:master"
    tags: "cbb_config:build_tester"
    tags: "cbb_display_label:TestLabel"
    properties: "cbb_branch:master"
    properties: "cbb_config:build_tester"
    properties: "cbb_display_label:TestLabel"
    properties: "cbb_extra_args:[\\"--buildbot\\"]"
  }
}

job {
  id: "build_triggered_a"
  acl_sets: "default"
  schedule: "triggered"
  buildbucket: {
    server: "cr-buildbucket.appspot.com"
    bucket: "luci.chromeos.general"
    builder: "ProdBuilder"
    tags: "cbb_branch:master"
    tags: "cbb_config:build_triggered_a"
    tags: "cbb_display_label:MockLabel"
    properties: "cbb_branch:master"
    properties: "cbb_config:build_triggered_a"
    properties: "cbb_display_label:MockLabel"
    properties: "cbb_extra_args:[\\"--buildbot\\"]"
  }
}

job {
  id: "build_triggered_b"
  acl_sets: "default"
  schedule: "triggered"
  buildbucket: {
    server: "cr-buildbucket.appspot.com"
    bucket: "luci.chromeos.general"
    builder: "ProdBuilder"
    tags: "cbb_branch:master"
    tags: "cbb_config:build_triggered_b"
    tags: "cbb_display_label:MockLabel"
    properties: "cbb_branch:master"
    properties: "cbb_config:build_triggered_b"
    properties: "cbb_display_label:MockLabel"
    properties: "cbb_extra_args:[\\"--buildbot\\"]"
  }
}

job {
  id: "test-branch-branch_tester"
  acl_sets: "default"
  schedule: "run daily"
  buildbucket: {
    server: "cr-buildbucket.appspot.com"
    bucket: "luci.chromeos.general"
    builder: "TestBuilder"
    tags: "cbb_branch:test-branch"
    tags: "cbb_config:branch_tester"
    tags: "cbb_display_label:TestLabel"
    properties: "cbb_branch:test-branch"
    properties: "cbb_config:branch_tester"
    properties: "cbb_display_label:TestLabel"
    properties: "cbb_extra_args:[\\"--buildbot\\"]"
  }
}
'''
    result = gen_luci_scheduler.genLuciSchedulerConfig(
        site_config, branch_configs)

    self.assertEqual(result, expected)
