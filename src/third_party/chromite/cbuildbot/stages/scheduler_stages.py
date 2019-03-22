# -*- coding: utf-8 -*-
# Copyright 2016 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Module containing the scheduler stages."""

from __future__ import print_function

import os
import time

from chromite.cbuildbot.stages import generic_stages
from chromite.lib import buildbucket_lib
from chromite.lib import build_requests
from chromite.lib import constants
from chromite.lib import config_lib
from chromite.lib import cros_logging as logging
from chromite.lib import failures_lib
from chromite.lib import request_build
from chromite.lib.const import waterfall


def BuilderName(build_config, active_waterfall, current_builder):
  """Gets the corresponding builder name of the build.

  Args:
    build_config: build config (string) of the build.
    active_waterfall: active waterfall to run the build.
    current_builder: buildbot builder name of the current builder, or None.

  Returns:
    Builder name to run the build on.
  """
  # The builder name is configured differently for release builds in
  # chromeos and chromeos_release waterfalls. (see crbug.com/755276)
  if active_waterfall == waterfall.WATERFALL_RELEASE:
    assert current_builder
    # Example: master-release release-R64-10176.B
    named_branch = current_builder.split()[1]
    return '%s %s' % (build_config, named_branch)
  else:
    return build_config


class ScheduleSlavesStage(generic_stages.BuilderStage):
  """Stage that schedules slaves for the master build."""

  category = constants.CI_INFRA_STAGE

  def __init__(self, builder_run, sync_stage, **kwargs):
    super(ScheduleSlavesStage, self).__init__(builder_run, **kwargs)
    self.sync_stage = sync_stage
    self.buildbucket_client = self.GetBuildbucketClient()

  def _GetBuildbucketBucket(self, build_name, build_config):
    """Get the corresponding Buildbucket bucket.

    Args:
      build_name: name of the build to put to Buildbucket.
      build_config: config of the build to put to Buildbucket.

    Raises:
      NoBuildbucketBucketFoundException when no Buildbucket bucket found.
    """
    bucket = buildbucket_lib.WATERFALL_BUCKET_MAP.get(
        build_config.active_waterfall)

    if bucket is None:
      raise buildbucket_lib.NoBuildbucketBucketFoundException(
          'No Buildbucket bucket found for builder %s waterfall: %s' %
          (build_name, build_config.active_waterfall))

    return bucket

  def _FindMostRecentBotId(self, build_config, branch):
    buildbucket_client = self.GetBuildbucketClient()
    if not buildbucket_client:
      logging.info('No buildbucket_client, no bot found.')
      return None

    previous_builds = buildbucket_client.SearchAllBuilds(
        self._run.options.debug,
        buckets=constants.ACTIVE_BUCKETS,
        limit=1,
        tags=['cbb_config:%s' % build_config,
              'cbb_branch:%s' % branch],
        status=constants.BUILDBUCKET_BUILDER_STATUS_COMPLETED)

    if not previous_builds:
      logging.info('No previous build found, no bot found.')
      return None

    bot_id = buildbucket_lib.GetBotId(previous_builds[0])
    if not bot_id:
      logging.info('Previous build has no bot.')
      return None

    return bot_id

  def PostSlaveBuildToBuildbucket(self, build_name, build_config,
                                  master_build_id, master_buildbucket_id,
                                  dryrun=False):
    """Send a Put slave build request to Buildbucket.

    Args:
      build_name: Slave build name to put to Buildbucket.
      build_config: Slave build config to put to Buildbucket.
      master_build_id: CIDB id of the master scheduling the slave build.
      master_buildbucket_id: buildbucket id of the master scheduling the
                             slave build.
      dryrun: Whether a dryrun, default to False.

    Returns:
      Tuple:
        buildbucket_id
        created_ts
    """
    bucket = self._GetBuildbucketBucket(build_name, build_config)

    luci_builder = None
    requested_bot = None
    if bucket == constants.INTERNAL_SWARMING_BUILDBUCKET_BUCKET:
      if build_config.build_affinity:
        requested_bot = self._FindMostRecentBotId(build_config.name,
                                                  self._run.manifest_branch)
    else:
      # If it's not a swarming build, we must explicitly set this to the
      # waterfall column name.
      current_buildername = os.environ.get('BUILDBOT_BUILDERNAME', None)
      luci_builder = BuilderName(
          build_name, build_config.active_waterfall, current_buildername)

    if requested_bot:
      logging.info('Requesting build affinity for %s against %s',
                   build_config.name, requested_bot)

    request = request_build.RequestBuild(
        build_config=build_name,
        luci_builder=luci_builder,
        display_label=build_config.display_label,
        branch=self._run.manifest_branch,
        master_cidb_id=master_build_id,
        master_buildbucket_id=master_buildbucket_id,
        bucket=bucket,
        extra_args=['--buildbot'],
        requested_bot=requested_bot,
    )
    result = request.Submit(dryrun=dryrun)

    logging.info('Build_name %s buildbucket_id %s created_timestamp %s',
                 result.build_config, result.buildbucket_id, result.created_ts)
    logging.PrintBuildbotLink(result.build_config, result.url)

    return (result.buildbucket_id, result.created_ts)

  def ScheduleSlaveBuildsViaBuildbucket(self, important_only=False,
                                        dryrun=False):
    """Schedule slave builds by sending PUT requests to Buildbucket.

    Args:
      important_only: Whether only schedule important slave builds, default to
        False.
      dryrun: Whether a dryrun, default to False.
    """
    if self.buildbucket_client is None:
      logging.info('No buildbucket_client. Skip scheduling slaves.')
      return

    build_id, db = self._run.GetCIDBHandle()
    if build_id is None:
      logging.info('No build id. Skip scheduling slaves.')
      return

    # May be None. This is okay.
    master_buildbucket_id = self._run.options.buildbucket_id

    scheduled_important_slave_builds = []
    scheduled_experimental_slave_builds = []
    unscheduled_slave_builds = []
    scheduled_build_reqs = []

    # Get all active slave build configs.
    slave_config_map = self._GetSlaveConfigMap(important_only)
    for slave_config_name, slave_config in sorted(slave_config_map.iteritems()):
      try:
        buildbucket_id, created_ts = self.PostSlaveBuildToBuildbucket(
            slave_config_name, slave_config, build_id, master_buildbucket_id,
            dryrun=dryrun)
        request_reason = None

        if slave_config.important:
          scheduled_important_slave_builds.append(
              (slave_config_name, buildbucket_id, created_ts))
          request_reason = build_requests.REASON_IMPORTANT_CQ_SLAVE
        else:
          scheduled_experimental_slave_builds.append(
              (slave_config_name, buildbucket_id, created_ts))
          request_reason = build_requests.REASON_EXPERIMENTAL_CQ_SLAVE

        scheduled_build_reqs.append(build_requests.BuildRequest(
            None, build_id, slave_config_name, None, buildbucket_id,
            request_reason, None))
      except buildbucket_lib.BuildbucketResponseException as e:
        # Use 16-digit ts to be consistent with the created_ts from Buildbucket
        current_ts = int(round(time.time() * 1000000))
        unscheduled_slave_builds.append((slave_config_name, None, current_ts))
        if important_only or slave_config.important:
          raise
        else:
          logging.warning('Failed to schedule %s current timestamp %s: %s',
                          slave_config_name, current_ts, e)

    if config_lib.IsMasterCQ(self._run.config) and db and scheduled_build_reqs:
      db.InsertBuildRequests(scheduled_build_reqs)

    self._run.attrs.metadata.ExtendKeyListWithList(
        constants.METADATA_SCHEDULED_IMPORTANT_SLAVES,
        scheduled_important_slave_builds)
    self._run.attrs.metadata.ExtendKeyListWithList(
        constants.METADATA_SCHEDULED_EXPERIMENTAL_SLAVES,
        scheduled_experimental_slave_builds)
    self._run.attrs.metadata.ExtendKeyListWithList(
        constants.METADATA_UNSCHEDULED_SLAVES, unscheduled_slave_builds)

  @failures_lib.SetFailureType(failures_lib.InfrastructureFailure)
  def PerformStage(self):
    if (config_lib.IsMasterCQ(self._run.config) and
        not self.sync_stage.pool.HasPickedUpCLs()):
      logging.info('No new CLs or chumpped CLs found to verify in this CQ run,'
                   'do not schedule CQ slaves.')
      return

    self.ScheduleSlaveBuildsViaBuildbucket(important_only=False,
                                           dryrun=self._run.options.debug)
