# -*- coding: utf-8 -*-
# Copyright 2018 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Module containing firmware builders."""

from __future__ import print_function

from chromite.cbuildbot.builders import generic_builders
from chromite.cbuildbot.stages import branch_archive_stages
from chromite.cbuildbot.stages import workspace_stages


class BuildSpecBuilder(generic_builders.Builder):
  """Builder that generates new buildspecs.

  This build does four things.
    1) Uprev and commit ebuilds based on TOT.
    2) Increatement the ChromeOS version number.
    3) Generate a buildspec based on that version number.
    4) Launch child builds based on the buildspec.
  """

  def GetSyncInstance(self):
    """Returns an instance of a SyncStage that should be run."""
    return self._GetStageInstance(workspace_stages.WorkspaceSyncStage,
                                  build_root=self._run.options.workspace)

  def RunStages(self):
    """Run the stages."""

    if not self._run.options.force_version:
      # If we were not given a specific buildspec to build, create one.
      self._RunStage(workspace_stages.WorkspaceUprevAndPublishStage,
                     build_root=self._run.options.workspace)

      self._RunStage(workspace_stages.WorkspacePublishBuildspecStage,
                     build_root=self._run.options.workspace)

    if self._run.config.slave_configs:
      # If there are child builds to schedule, schedule them.
      self._RunStage(workspace_stages.WorkspaceScheduleChildrenStage,
                     build_root=self._run.options.workspace)


class FirmwareBranchBuilder(BuildSpecBuilder):
  """Builder that builds firmware branches.

  This builder checks out a second copy of ChromeOS into the workspace
  on the firmware branch, and performs a firmware build there for 1 or
  more boards, and publishes all of the results to both it's own build
  artifacts, as well as to a second location specific to each board
  that looks like a the results of a traditional single board firmware
  build.
  """

  def RunStages(self):
    """Run the stages."""
    super(FirmwareBranchBuilder, self).RunStages()

    self._RunStage(workspace_stages.WorkspaceInitSDKStage,
                   build_root=self._run.options.workspace)

    self._RunStage(workspace_stages.WorkspaceUpdateSDKStage,
                   build_root=self._run.options.workspace)

    for board in self._run.config.boards:
      self._RunStage(workspace_stages.WorkspaceSetupBoardStage,
                     build_root=self._run.options.workspace,
                     board=board)

      self._RunStage(workspace_stages.WorkspaceBuildPackagesStage,
                     build_root=self._run.options.workspace,
                     board=board)

      self._RunStage(branch_archive_stages.FirmwareArchiveStage,
                     build_root=self._run.options.workspace,
                     board=board)


class FactoryBranchBuilder(BuildSpecBuilder):
  """Builder that builds factory branches.

  This builder checks out a second copy of ChromeOS into the workspace
  on the firmware branch, and performs a factory build there for 1
  board.
  """

  def RunStages(self):
    """Run the stages."""
    super(FactoryBranchBuilder, self).RunStages()

    assert len(self._run.config.boards) == 1
    board = self._run.config.boards[0]

    self._RunStage(workspace_stages.WorkspaceInitSDKStage,
                   build_root=self._run.options.workspace)

    self._RunStage(workspace_stages.WorkspaceUpdateSDKStage,
                   build_root=self._run.options.workspace)

    self._RunStage(workspace_stages.WorkspaceSyncChromeStage,
                   build_root=self._run.options.workspace)

    self._RunStage(workspace_stages.WorkspaceSetupBoardStage,
                   build_root=self._run.options.workspace,
                   board=board)

    self._RunStage(workspace_stages.WorkspaceBuildPackagesStage,
                   build_root=self._run.options.workspace,
                   board=board)

    self._RunStage(workspace_stages.WorkspaceUnitTestStage,
                   build_root=self._run.options.workspace,
                   board=board)

    self._RunStage(workspace_stages.WorkspaceBuildImageStage,
                   build_root=self._run.options.workspace,
                   board=board)

    self._RunStage(workspace_stages.WorkspaceDebugSymbolsStage,
                   build_root=self._run.options.workspace,
                   board=board)

    self._RunStage(branch_archive_stages.FactoryArchiveStage,
                   build_root=self._run.options.workspace,
                   board=board)
