# -*- coding: utf-8 -*-
# Copyright 2018 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# pylint: disable=line-too-long
"""Generate LUCI Scheduler config file.

This generates the LUCI Scheduler configuration file for ChromeOS builds based
on the chromeos_config contents.

Changes to chromite/config/luci-scheduler.cfg will be autodeployed:
  http://cros-goldeneye/chromeos/legoland/builderHistory?buildConfig=luci-scheduler-updater

Notes:
  Normal builds are scheduled based on the builder values for
  'schedule' and 'triggered_gitiles' in config/chromeos_config.py.

  Branched builds are scheduled based on the function
  chromeos_config.BranchScheduleConfig()
"""
# pylint: enable=line-too-long

from __future__ import print_function

import sys

from chromite.config import chromeos_config
from chromite.lib import commandline
from chromite.lib import config_lib


_CONFIG_HEADER = """# Defines buckets on luci-scheduler.appspot.com.
#
# For schema of this file and documentation see ProjectConfig message in
# https://github.com/luci/luci-go/blob/master/scheduler/appengine/messages/config.proto

# Generated with chromite/scripts/gen_luci_scheduler

# Autodeployed with:
# http://cros-goldeneye/chromeos/legoland/builderHistory?buildConfig=luci-scheduler-updater

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
  acls {
    role: TRIGGERER
    granted_to: "group:mdb/chromeos-build-access"
  }
}
"""

def buildJobName(build_config):
  if 'schedule_branch' in build_config:
    return '%s-%s' % (build_config.schedule_branch, build_config.name)
  else:
    return build_config.name


def genSchedulerJob(build_config):
  """Generate the luci scheduler job for a given build config.

  Args:
    build_config: config_lib.BuildConfig.

  Returns:
    Multiline string to include in the luci scheduler configuration.
  """
  job_name = buildJobName(build_config)
  if 'schedule_branch' in build_config:
    branch = build_config.schedule_branch
  else:
    branch = 'master'

  tags = {
      'cbb_branch': branch,
      'cbb_config': build_config.name,
      'cbb_display_label': build_config.display_label,
      'cbb_workspace_branch': build_config.workspace_branch,
      'cbb_goma_client_type': build_config.goma_client_type,
  }

  # Filter out tags with no value set.
  tags = {k: v for k, v in tags.iteritems() if v}


  tag_lines = ['    tags: "%s:%s"' % (k, tags[k])
               for k in sorted(tags.keys())]
  prop_lines = ['    properties: "%s:%s"' % (k, tags[k])
                for k in sorted(tags.keys())]

  # TODO: Move --buildbot arg into recipe, and remove from here.
  template = """
job {
  id: "%(job_name)s"
  acl_sets: "default"
  schedule: "%(schedule)s"
  buildbucket: {
    server: "cr-buildbucket.appspot.com"
    bucket: "luci.chromeos.general"
    builder: "%(builder)s"
%(tag_lines)s
%(prop_lines)s
    properties: "cbb_extra_args:[\\"--buildbot\\"]"
  }
}
"""

  return template % {
      'job_name': job_name,
      'builder': build_config.luci_builder,
      'schedule': build_config.schedule,
      'tag_lines': '\n'.join(tag_lines),
      'prop_lines': '\n'.join(prop_lines),
  }


def genSchedulerTrigger(trigger_name, repo, refs, builds):
  """Generate the luci scheduler job for a given build config.

  Args:
    trigger_name: Name of the trigger as a string.
    repo: Gitiles URL git git repository.
    refs: Iterable of git refs to check. May use regular expressions.
    builds: Iterable of build config names to trigger.

  Returns:
    Multiline string to include in the luci scheduler configuration.
  """
  template = """
trigger {
  id: "%(trigger_name)s"
  acl_sets: "default"
  schedule: "with 5m interval"
  gitiles: {
    repo: "%(repo)s"
%(refs)s
  }
%(triggers)s
}
"""

  return template % {
      'trigger_name': trigger_name,
      'repo': repo,
      'refs': '\n'.join('    refs: "%s"' % r for r in refs),
      'triggers': '\n'.join('  triggers: "%s"' % b for b in builds),
  }


def genLuciSchedulerConfig(site_config, branch_config):
  """Generate a luciSchedulerConfig as a string.

  Args:
    site_config: A config_lib.SiteConfig instance.
    branch_config: A list of BuildConfig instances to schedule.

  Returns:
    The complete scheduler configuration contents as a string.
  """
  # Trigger collection is used to collect together trigger information, so
  # we can reuse the same trigger for multiple builds as needed.
  # It maps gitiles_key to a set of build_names.
  # A gitiles_key = (gitiles_url, tuple(ref_list))
  trigger_collection = {}

  jobs = []

  # Order the configs consistently.
  configs = [site_config[name] for name in sorted(site_config)] + branch_config

  for config in configs:
    # Populate jobs.
    if config.schedule:
      jobs.append(genSchedulerJob(config))

    # Populate trigger_collection.
    if config.triggered_gitiles:
      for gitiles_url, ref_list in config.triggered_gitiles:
        gitiles_key = (gitiles_url, tuple(ref_list))
        trigger_collection.setdefault(gitiles_key, set())
        trigger_collection[gitiles_key].add(buildJobName(config))

  # Populate triggers.
  triggers = []
  trigger_counter = 0
  for gitiles_key in sorted(trigger_collection):
    builds = sorted(trigger_collection[gitiles_key])

    trigger_name = 'trigger_%s' % trigger_counter
    gitiles_url, refs = gitiles_key
    triggers.append(genSchedulerTrigger(
        trigger_name, gitiles_url, refs, builds))
    trigger_counter += 1

  return ''.join([_CONFIG_HEADER] + triggers + jobs)


def GetParser():
  """Creates the argparse parser."""
  parser = commandline.ArgumentParser(description=__doc__)

  parser.add_argument('-o', '--file_out', type='path',
                      help='Write output to specified file.')

  return parser


def main(argv):
  parser = GetParser()
  options = parser.parse_args(argv)
  options.Freeze()

  site_config = config_lib.GetConfig()
  branch_config = chromeos_config.BranchScheduleConfig()

  with (open(options.file_out, 'w') if options.file_out else sys.stdout) as fh:
    fh.write(genLuciSchedulerConfig(site_config, branch_config))
