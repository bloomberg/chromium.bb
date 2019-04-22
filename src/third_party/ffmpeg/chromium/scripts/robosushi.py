#!/usr/bin/python
#
# Copyright 2018 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
#
# Set up everything for the roll.
#
# --setup: set up the host to do a roll.  Idempotent, but probably doesn't need
#          to be run more than once in a while.
# --test:  configure ffmpeg for the host machine, and try running media unit
#          tests and the ffmpeg regression tests.
# --build: build ffmpeg configs for all platforms, then generate gn config.
# --all:   do everything.  Right now, this is the same as "--test --build".

import getopt
import os
import sys

import robo_branch
from robo_lib import log
import robo_lib
import robo_build
import robo_setup

def main(argv):
  robo_configuration = robo_lib.RoboConfiguration()
  robo_configuration.chdir_to_ffmpeg_home();

  parsed, remaining = getopt.getopt(argv, "",
          ["setup", "test", "build", "auto-merge"])

  for opt, arg in parsed:
    if opt == "--setup":
      robo_setup.InstallPrereqs(robo_configuration)
      robo_setup.EnsureToolchains(robo_configuration)
      robo_setup.EnsureASANDirWorks(robo_configuration)
      robo_setup.EnsureChromiumNasm(robo_configuration)
    elif opt == "--test":
      robo_build.BuildAndImportFFmpegConfigForHost(robo_configuration)
      robo_build.RunTests(robo_configuration)
    elif opt == "--build":
      # Unconditionally build all the configs and import them.
      robo_build.BuildAndImportAllFFmpegConfigs(robo_configuration)
    elif opt == "--auto-merge":
      # Start a branch (if needed), merge (if needed), and try to verify it.
      # TODO: Verify that the working directory is clean.
      robo_branch.CreateAndCheckoutDatedSushiBranchIfNeeded(robo_configuration)
      robo_branch.MergeUpstreamToSushiBranchIfNeeded(robo_configuration)
      # We want to push the merge and make the local branch track it, so that
      # future 'git cl upload's don't try to review the merge commit, and spam
      # the ffmpeg committers.
      robo_branch.PushToOriginWithoutReviewAndTrackIfNeeded(robo_configuration)

      # Try to get everything to build if we haven't committed the configs yet.
      # Note that the only time we need to do this again is if some change makes
      # different files added / deleted to the build, or if ffmpeg configure
      # changes.  We don't need to do this if you just edit ffmpeg sources;
      # those will be built with the tests if they've changed since last time.
      #
      # So, if you're just editing ffmpeg sources to get tests to pass, then you
      # probably don't need to do this step again.
      #
      # TODO: Add a way to override this.  I guess just edit out the config
      # commit with a rebase for now.
      if robo_branch.IsCommitOnThisBranch(robo_configuration,
                                          robo_configuration.gn_commit_title()):
        log("Skipping config build since already committed")
      else:
        robo_build.BuildAndImportAllFFmpegConfigs(robo_configuration)
        # Run sanity checks on the merge before we commit.
        robo_branch.CheckMerge(robo_configuration)
        # Write the config changes to help the reviewer.
        robo_branch.WriteConfigChangesFile(robo_configuration)
        # TODO(liberato): Add the 'autodetect' regex too.
        # Handle autorenames last, so that we don't stage things and then fail.
        # While it's probably okay, it's nicer if we don't.
        robo_branch.HandleAutorename(robo_configuration)
        robo_branch.AddAndCommit(robo_configuration,
                                 robo_configuration.gn_commit_title())

      # Update the patches file.
      if robo_branch.IsCommitOnThisBranch(
          robo_configuration,
          robo_configuration.patches_commit_title()):
        log("Skipping patches file since already committed")
      else:
        robo_branch.WritePatchesReadme(robo_configuration)
        robo_branch.AddAndCommit(robo_configuration,
                                 robo_configuration.patches_commit_title())

      # Run the tests.  Note that this will re-run ninja from chromium/src,
      # which will rebuild any changed ffmpeg sources as it normally would.
      robo_build.RunTests(robo_configuration)

      # TODO: Start a fake deps roll.  To do this, we would:
      # Create new remote branch from the current remote sushi branch.
      # Create and check out a new local branch at the current local branch.
      # Make the new local branch track the new remote branch.
      # Push to origin/new remote branch.
      # Start a fake deps roll CL that runs the *san bots.
      # Switch back to original local branch.
      # For extra points, include a pointer to the fake deps roll CL in the
      # local branch, so that when it's pushed for review, it'll point the
      # reviewer at it.

      # TODO: git cl upload for review.
    else:
      raise Exception("Unknown option '%s'" % opt);

if __name__ == "__main__":
  main(sys.argv[1:])
