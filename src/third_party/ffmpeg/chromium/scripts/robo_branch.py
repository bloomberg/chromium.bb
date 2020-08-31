#!/usr/bin/python
#
# Copyright 2018 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
#
# Functions to facilitate a branching for merges.
#
# A "sushi branch" is a branch that we've created and manage.  We do this to
# prevent making changes to branches that we don't understand.  It's mostly as
# a sanity check that we're being used correctly.

import check_merge
from datetime import datetime
import find_patches
import os
import re
from robo_lib import UserInstructions
from robo_lib import log
from subprocess import check_output

def IsWorkingDirectoryClean():
  """Return true if and only if the working directory is clean."""
  return not check_output(["git", "status", "--untracked-files=no",
          "--porcelain"]).strip()

def RequiresCleanWorkingDirectory(fn):
  def wrapper(*args, **kwargs):
    if not IsWorkingDirectoryClean():
      raise Exception("Working directory is not clean.")
    fn(*args, **kwargs)
  return wrapper

@RequiresCleanWorkingDirectory
def CreateAndCheckoutDatedSushiBranch(cfg):
  """Create a dated branch from origin/master and check it out."""
  now = datetime.now()
  branch_name=cfg.sushi_branch_prefix() + now.strftime("%Y-%m-%d-%H-%M-%S")
  log("Creating dated branch %s" % branch_name)
  # Fetch the latest from origin
  if cfg.Call(["git", "fetch", "origin"]):
    raise Exception("Could not fetch from origin")

  # Create the named branch
  # Note that we definitely do not want this branch to track origin/master; that
  # would eventually cause 'git cl upload' to push the merge commit, assuming
  # that the merge commit is pushed to origin/sushi-branch.  One might argue
  # that we should push the merge to origin/master, which would make this okay.
  # For now, we leave the branch untracked to make sure that the user doesn't
  # accidentally do the wrong thing.  I think that with an automatic deps roll,
  # we'll want to stage things on origin/sushi-branch.
  #
  # We don't want to push anything to origin yet, though, just to keep from
  # making a bunch of sushi branches.  We can do it later just as easily.
  if cfg.Call(["git",
                             "branch",
                             "--no-track",
                             branch_name,
                             "origin/master"]):
    raise Exception("Could not create branch")

  # NOTE: we could push the remote branch back to origin and start tracking it
  # now, and not worry about tracking later.  However, until the scripts
  # actually work, i don't want to push a bunch of branches to origin.

  # Check out the branch.  On failure, delete the new branch.
  if cfg.Call(["git", "checkout", branch_name]):
    cfg.Call(["git", "branch", "-D", branch_name])
    raise Exception("Could not checkout branch")

  cfg.SetBranchName(branch_name)

def CreateAndCheckoutDatedSushiBranchIfNeeded(cfg):
  """Create a dated branch from origin/master if we're not already on one."""
  if cfg.sushi_branch_name():
    log("Already on sushi branch %s" % cfg.sushi_branch_name())
    return

  CreateAndCheckoutDatedSushiBranch(cfg)

@RequiresCleanWorkingDirectory
def MergeUpstreamToSushiBranch(cfg):
  log("Merging upstream/master to local branch")
  if not cfg.sushi_branch_name():
    raise Exception("Refusing to do a merge on a branch I didn't create")
  if cfg.Call(["git", "fetch", "upstream"]):
    raise Exception("Could not fetch from upstream")
  if cfg.Call(["git", "merge", "upstream/master"]):
    raise UserInstructions("Merge failed -- resolve conflicts manually.")
  log("Merge has completed successfully")

def GetMergeParentsIfAny(cfg):
  """Return the set of commit sha-1s of the merge commit, if one exists, between
  HEAD and where it joins up with origin/master.  Otherwise, return []."""
  # Get all sha1s between us and origin/master
  sha1s = check_output(["git", "log", "--format=%H",
          "origin/master..%s" % cfg.branch_name()]).split()
  for sha1 in sha1s:
    # Does |sha1| have more than one parent commit?
    parents = check_output(["git", "show", "--no-patch", "--format=%P",
         sha1]).split()
    if len(parents) > 1:
      return parents
  return []

def IsMergeCommitOnThisBranch(cfg):
  """Return true if there's a merge commit on this branch."""
  return GetMergeParentsIfAny(cfg) != []

def FindUpstreamMergeParent(cfg):
  """Return the sha-1 of the upstream side of the merge, if there is a merge
  commit on this branch.  Otherwise, fail."""
  sha1s = GetMergeParentsIfAny(cfg)
  for sha1 in sha1s:
    # 'not' is correct -- it returns zero if it is an ancestor => upstream.
    if not cfg.Call(["git", "merge-base", "--is-ancestor", sha1,
                     "upstream/master"]):
      return sha1
  raise Exception("No upstream merge parent found.  Is the merge committed?")

def MergeUpstreamToSushiBranchIfNeeded(cfg):
  """Start a merge if we've not started one before, or do nothing successfully
  if the merge is complete.  If it's half done, then get mad and exit."""
  if IsMergeCommitOnThisBranch(cfg):
    log("Merge commit already marked as complete")
    return
  # See if a merge is in progress.  "git merge HEAD" will do nothing if it
  # succeeds, but will fail if a merge is in progress.
  if cfg.Call(["git", "merge", "HEAD"]):
    raise UserInstructions(
      "Merge is in progress -- please resolve conflicts and complete it.")
  # There is no merge on this branch, and none is in progress.  Start a merge.
  MergeUpstreamToSushiBranch(cfg)

def CheckMerge(cfg):
  """Verify that the merge config looks good."""
  # If we haven't built all configs, then we might not be checking everything.
  # The check might look at config for each platform, etc.
  log("Checking merge for common failures")
  cfg.chdir_to_ffmpeg_home();
  check_merge.main([])

def WritePatchesReadme(cfg):
  """Write the chromium patches file."""
  log("Generating CHROMIUM.patches file")
  cfg.chdir_to_ffmpeg_home();
  with open(os.path.join("chromium", "patches", "README"), "w+") as f:
    find_patches.write_patches_file("HEAD", f)

def WriteConfigChangesFile(cfg):
  """Write a file that summarizes the config changes, for easier reviewing."""
  cfg.chdir_to_ffmpeg_home();
  # This looks for things that were added / deleted that look like #define or
  # %define (for asm) ending in 0 or 1, that have changed in any of the configs.
  os.system("git diff origin/master --unified=0 -- chromium/config/* |"
            "grep '^[+-].*[01]$' | sed -e 's/[%#]define//g' |sort |"
            "uniq -s 1 >chromium/patches/config_flag_changes.txt")

def AddAndCommit(cfg, commit_title):
  """Add everything, and commit locally with |commit_title|"""
  log("Creating local commit %s" % commit_title)
  cfg.chdir_to_ffmpeg_home();
  if IsWorkingDirectoryClean():
    log("No files to commit to %s" % commit_title)
    return
  # TODO: Ignore this file, for the "comment out autorename exception" thing.
  if cfg.Call(["git", "add", "-u"]):
    raise Exception("Could not add files")
  if cfg.Call(["git", "commit", "-m", commit_title]):
    raise Exception("Could create commit")

def IsTrackingBranchSet(cfg):
  """Check if the local branch is tracking upstream."""
  # git branch -vv --list ffmpeg_roll
  # ffmpeg_roll 28e7fbe889 [origin/master: behind 8859] Merge branch 'merge-m57'
  output = check_output(["git", "branch", "-vv", "--list",
                         cfg.sushi_branch_name()])
  # Note that it might have ": behind" or other things.
  return "[origin/%s" % cfg.sushi_branch_name() in output

def PushToOriginWithoutReviewAndTrackIfNeeded(cfg):
  """Push the local branch to origin/ if we haven't yet."""
  cfg.chdir_to_ffmpeg_home();
  # If the tracking branch is unset, then assume that we haven't done this yet.
  if IsTrackingBranchSet(cfg):
    log("Already have local tracking branch")
    return
  log("Pushing merge to origin without review")
  cfg.Call(["git", "push", "origin", cfg.sushi_branch_name()])
  log("Setting tracking branch")
  cfg.Call(["git", "branch", "--set-upstream-to=origin/%s" %
         cfg.sushi_branch_name()])
  # Sanity check.  We don't want to start pushing other commits without review.
  if not IsTrackingBranchSet(cfg):
    raise Exception("Tracking branch is not set, but I just set it!")

def HandleAutorename(cfg):
  # We assume that there is a script written by generate_gn.py that adds /
  # removes files needed for autorenames.  Run it.
  log("Updating git for any autorename changes")
  cfg.chdir_to_ffmpeg_home();
  if cfg.Call(["chmod", "+x", cfg.autorename_git_file()]):
    raise Exception("Unable to chmod %s" % cfg.autorename_git_file())
  if cfg.Call([cfg.autorename_git_file()]):
    raise Exception("Unable to run %s" % cfg.autorename_git_file())

def IsCommitOnThisBranch(robo_configuration, commit_title):
  """Detect if we've already committed the |commit_title| locally."""
  # Get all commit titles between us and origin/master
  titles = check_output(["git", "log", "--format=%s",
          "origin/master..%s" % robo_configuration.branch_name()])
  return commit_title in titles

def IsPatchesFileDone(robo_configuration):
  """Return False if and only if the patches file isn't checked in."""
  if IsCommitOnThisBranch(
                          robo_configuration,
                          robo_configuration.patches_commit_title()):
    log("Skipping patches file since already committed")
    return True
  return False

@RequiresCleanWorkingDirectory
def UpdatePatchesFileUnconditionally(robo_configuration):
  """Update the patches file."""
  WritePatchesReadme(robo_configuration)
  AddAndCommit(robo_configuration,
               robo_configuration.patches_commit_title())

def IsChromiumReadmeDone(robo_configuration):
  """Return False if and only if README.chromium isn't checked in."""
  if IsCommitOnThisBranch(
                          robo_configuration,
                          robo_configuration.readme_chromium_commit_title()):
    log("Skipping README.chromium file since already committed")
    return True
  return False

@RequiresCleanWorkingDirectory
def UpdateChromiumReadmeWithUpstream(robo_configuration):
  """Update the upstream info in README.chromium and commit the result."""
  log("Updating merge info in README.chromium")
  merge_sha1 = FindUpstreamMergeParent(robo_configuration)
  robo_configuration.chdir_to_ffmpeg_home();
  with open("README.chromium", "r+") as f:
    readme = f.read()
  last_upstream_merge = "Last Upstream Merge:"
  merge_date = check_output(["git", "log", "-1","--date=format:%b %d %Y",
                            "--format=%cd", merge_sha1])
  readme = re.sub(r"(Last Upstream Merge:).*\n",
                  r"\1 %s, %s" % (merge_sha1, merge_date),
                  readme)
  with open("README.chromium", "w") as f:
    f.write(readme)
  AddAndCommit(robo_configuration,
               robo_configuration.readme_chromium_commit_title())

def HasGerritIssueNumber(robo_configuration):
  """Return True if and only if this branch has been pushed for review."""
  robo_configuration.chdir_to_ffmpeg_home();
  return os.system(
          "git cl issue 2>/dev/null |grep Issue |grep None >/dev/null") != 0

def IsUploadedForReview(robo_configuration):
  """Check if the local branch is already uploaded."""
  robo_configuration.chdir_to_ffmpeg_home();
  if not HasGerritIssueNumber(robo_configuration):
    log("No Gerrit issue number exsts.")
    return False

  if not IsWorkingDirectoryClean():
    log("Working directory is not clean -- commit changes and update CL");
    return False

  # Has been uploaded for review.  Might or might not have been landed yet.
  return True

def IsUploadedForReviewAndLanded(robo_configuration):
  """Check if the local sushi branch has been uploaded for review, and has also
  been landed."""
  robo_configuration.chdir_to_ffmpeg_home();
  if not IsUploadedForReview(robo_configuration):
    log("Is not uploaded for review")
    return False
  # See if origin/sushi and local/sushi are the same.  This check by itself
  # isn't sufficient, since it would return true any time the two are in sync.
  diff = check_output(["git", "diff",
               "origin/" + robo_configuration.sushi_branch_name(),
               robo_configuration.sushi_branch_name()]).strip()
  return not diff

@RequiresCleanWorkingDirectory
def UploadForReview(robo_configuration):
  """Assuming that tests pass (we can't check), upload to review."""
  robo_configuration.chdir_to_ffmpeg_home();
  if IsUploadedForReview(robo_configuration):
    raise Exception(
            "Sushi branch is already uploaded for review!  (try git cl web)")
  log("Uploading sushi branch for review.")
  os.system("git cl upload")

@RequiresCleanWorkingDirectory
def TryFakeDepsRoll(robo_configuration):
  """Start a deps roll against the sushi branch, and -1 it."""
  log("Considering starting a fake deps roll")

  # Make sure that we've landed the sushi commits.  Note that this can happen if
  # somebody re-runs robosushi after we upload the commits to Gerrit, but before
  # they've been reviewed and landed.  This way, we provide a meaningful error.
  if not IsUploadedForReviewAndLanded(robo_configuration):
    raise Exception("Cannot start a fake deps roll until gerrit review lands!")

  robo_configuration.chdir_to_ffmpeg_home();
  sha1 = check_output("git", "show", "-1", "--format=%P").strip()
  if not sha1:
    raise Exception("Cannot get sha1 of HEAD for fakes dep roll")

  robo_configuration.chdir_to_chrome_src()
  # TODO: make sure that there's not a deps roll in progress, else we'll keep
  # doing this every time we're run.
  # TODO: get mad otherwise.
  check_output(["roll-deps.py", "third_party/ffmpeg", sha1])
  # TODO: -1 it.
