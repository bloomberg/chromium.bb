# Copyright 2016 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""This module uprevs Android for cbuildbot.

After calling, it prints outs ANDROID_VERSION_ATOM=(version atom string).  A
caller could then use this atom with emerge to build the newly uprevved version
of Android e.g.

./cros_mark_android_as_stable
Returns chromeos-base/android-container-2559197

emerge-veyron_minnie-cheets =chromeos-base/android-container-2559197-r1
"""

from __future__ import print_function

import filecmp
import glob
import os
import re

from chromite.cbuildbot import constants
from chromite.lib import commandline
from chromite.lib import cros_build_lib
from chromite.lib import cros_logging as logging
from chromite.lib import git
from chromite.lib import gs
from chromite.lib import portage_util
from chromite.scripts import cros_mark_as_stable


# Dir where all the action happens.
_OVERLAY_DIR = '%(srcroot)s/private-overlays/project-cheets-private/'

_GIT_COMMIT_MESSAGE = ('Marking latest for %(android_pn)s ebuild '
                       'with version %(android_version)s as stable.')

# URLs that print lists of Android revisions between two build ids.
_ANDROID_VERSION_URL = ('http://android-build-uber.corp.google.com/repo.html?'
                        'last_bid=%(old)s&bid=%(new)s&branch=%(branch)s')


def IsBuildIdValid(bucket_url, build_branch, build_id):
  """Checks that a specific build_id is valid.

  Looks for that build_id for all builds. Confirms that the subpath can
  be found and that the zip file is present in that subdirectory.

  Args:
    bucket_url: URL of Android build gs bucket
    build_branch: branch of Android builds
    build_id: A string. The Android build id number to check.

  Returns:
    Returns subpaths dictionary if build_id is valid.
    None if the build_id is not valid.
  """
  gs_context = gs.GSContext()
  subpaths_dict = {}
  for build, (target, _) in constants.ANDROID_BUILD_TARGETS.iteritems():
    build_dir = '%s-%s' % (build_branch, target)
    build_id_path = os.path.join(bucket_url, build_dir, build_id)

    # Find name of subpath.
    try:
      subpaths = gs_context.List(build_id_path)
    except gs.GSNoSuchKey:
      logging.warn(
          'Directory [%s] does not contain any subpath, ignoring it.',
          build_id_path)
      return None
    if len(subpaths) > 1:
      logging.warn(
          'Directory [%s] contains more than one subpath, ignoring it.',
          build_id_path)
      return None

    subpath_dir = subpaths[0].url.rstrip('/')
    subpath_name = os.path.basename(subpath_dir)

    # Look for a zipfile ending in the build_id number.
    try:
      gs_context.List(subpath_dir)
    except gs.GSNoSuchKey:
      logging.warn(
          'Did not find a file for build id [%s] in directory [%s].',
          build_id, subpath_dir)
      return None

    # Record subpath for the build.
    subpaths_dict[build] = subpath_name

  # If we got here, it means we found an appropriate build for all platforms.
  return subpaths_dict


def GetLatestBuild(bucket_url, build_branch):
  """Searches the gs bucket for the latest green build.

  Args:
    bucket_url: URL of Android build gs bucket
    build_branch: branch of Android builds

  Returns:
    Tuple of (latest version string, subpaths dictionary)
    If no latest build can be found, returns None, None
  """
  gs_context = gs.GSContext()
  common_build_ids = None
  # Find builds for each target.
  for target, _ in constants.ANDROID_BUILD_TARGETS.itervalues():
    build_dir = '-'.join((build_branch, target))
    base_path = os.path.join(bucket_url, build_dir)
    build_ids = []
    for gs_result in gs_context.List(base_path):
      # Remove trailing slashes and get the base name, which is the build_id.
      build_id = os.path.basename(gs_result.url.rstrip('/'))
      if not build_id.isdigit():
        logging.warn('Directory [%s] does not look like a valid build_id.',
                     gs_result.url)
        continue
      build_ids.append(build_id)

    # Update current list of builds.
    if common_build_ids is None:
      # First run, populate it with the first platform.
      common_build_ids = set(build_ids)
    else:
      # Already populated, find the ones that are common.
      common_build_ids.intersection_update(build_ids)

  if common_build_ids is None:
    logging.warn('Did not find a build_id common to all platforms.')
    return None, None

  # Otherwise, find the most recent one that is valid.
  for build_id in sorted(common_build_ids, key=int, reverse=True):
    subpaths = IsBuildIdValid(bucket_url, build_branch, build_id)
    if subpaths:
      return build_id, subpaths

  # If not found, no build_id is valid.
  logging.warn('Did not find a build_id valid on all platforms.')
  return None, None


def FindAndroidCandidates(package_dir):
  """Return a tuple of Android's unstable ebuild and stable ebuilds.

  Args:
    package_dir: The path to where the package ebuild is stored.

  Returns:
    Tuple [unstable_ebuild, stable_ebuilds].

  Raises:
    Exception: if no unstable ebuild exists for Android.
  """
  stable_ebuilds = []
  unstable_ebuilds = []
  for path in glob.glob(os.path.join(package_dir, '*.ebuild')):
    ebuild = portage_util.EBuild(path)
    if ebuild.version == '9999':
      unstable_ebuilds.append(ebuild)
    else:
      stable_ebuilds.append(ebuild)

  # Apply some sanity checks.
  if not unstable_ebuilds:
    raise Exception('Missing 9999 ebuild for %s' % package_dir)
  if not stable_ebuilds:
    logging.warning('Missing stable ebuild for %s' % package_dir)

  return portage_util.BestEBuild(unstable_ebuilds), stable_ebuilds


def CopyToArcBucket(android_bucket_url, build_branch, build_id, subpaths,
                    arc_bucket_url, acls):
  """Copies from source Android bucket to ARC++ specific bucket.

  Copies each build to the ARC bucket eliminating the subpath.
  Applies build specific ACLs for each file.

  Args:
    android_bucket_url: URL of Android build gs bucket
    build_branch: branch of Android builds
    build_id: A string. The Android build id number to check.
    subpaths: Subpath dictionary for each build to copy.
    arc_bucket_url: URL of the target ARC build gs bucket
    acls: ACLs dictionary for each build to copy.
  """
  gs_context = gs.GSContext()
  for build, subpath in subpaths.iteritems():
    target, pattern = constants.ANDROID_BUILD_TARGETS[build]
    build_dir = '%s-%s' % (build_branch, target)
    android_dir = os.path.join(android_bucket_url, build_dir, build_id, subpath)
    arc_dir = os.path.join(arc_bucket_url, build_dir, build_id)

    # Copy all target files from android_dir to arc_dir, setting ACLs.
    for targetfile in gs_context.List(android_dir):
      if re.search(pattern, targetfile.url):
        basename = os.path.basename(targetfile.url)
        arc_path = os.path.join(arc_dir, basename)
        acl = acls[build]
        needs_copy = True

        # Check a pre-existing file with the original source.
        if gs_context.Exists(arc_path):
          if (gs_context.Stat(targetfile.url).hash_crc32c !=
              gs_context.Stat(arc_path).hash_crc32c):
            logging.warn('Removing incorrect file %s', arc_path)
            gs_context.Remove(arc_path)
          else:
            logging.info('Skipping already copied file %s', arc_path)
            needs_copy = False

        # Copy if necessary, and set the ACL unconditionally.
        # The Stat() call above doesn't verify the ACL is correct and
        # the ChangeACL should be relatively cheap compared to the copy.
        # This covers the following caes:
        # - handling an interrupted copy from a previous run.
        # - rerunning the copy in case one of the googlestorage_acl_X.txt
        #   files changes (e.g. we add a new variant which reuses a build).
        if needs_copy:
          logging.info('Copying %s -> %s (acl %s)',
                       targetfile.url, arc_path, acl)
          gs_context.Copy(targetfile.url, arc_path, version=0)
        gs_context.ChangeACL(arc_path, acl_args_file=acl)


def MakeAclDict(package_dir):
  """Creates a dictionary of acl files for each build type.

  Args:
    package_dir: The path to where the package acl files are stored.

  Returns:
    Returns acls dictionary.
  """
  return dict(
      (k, os.path.join(package_dir, v))
      for k, v in constants.ARC_BUCKET_ACLS.items()
  )


def GetAndroidRevisionListLink(build_branch, old_android, new_android):
  """Returns a link to the list of revisions between two Android versions

  Given two AndroidEBuilds, generate a link to a page that prints the
  Android changes between those two revisions, inclusive.

  Args:
    build_branch: branch of Android builds
    old_android: ebuild for the version to diff from
    new_android: ebuild for the version to which to diff

  Returns:
    The desired URL.
  """
  return _ANDROID_VERSION_URL % {'branch': build_branch,
                                 'old': old_android.version,
                                 'new': new_android.version}


def MarkAndroidEBuildAsStable(stable_candidate, unstable_ebuild, android_pn,
                              android_version, package_dir, build_branch,
                              arc_bucket_url):
  r"""Uprevs the Android ebuild.

  This is the main function that uprevs from a stable candidate
  to its new version.

  Args:
    stable_candidate: ebuild that corresponds to the stable ebuild we are
      revving from.  If None, builds the a new ebuild given the version
      with revision set to 1.
    unstable_ebuild: ebuild corresponding to the unstable ebuild for Android.
    android_pn: package name.
    android_version: The \d+ build id of Android.
    package_dir: Path to the android-container package dir.
    build_branch: branch of Android builds.
    arc_bucket_url: URL of the target ARC build gs bucket.

  Returns:
    Full portage version atom (including rc's, etc) that was revved.
  """
  def IsTheNewEBuildRedundant(new_ebuild, stable_ebuild):
    """Returns True if the new ebuild is redundant.

    This is True if there if the current stable ebuild is the exact same copy
    of the new one.
    """
    if not stable_ebuild:
      return False

    if stable_candidate.version_no_rev == new_ebuild.version_no_rev:
      return filecmp.cmp(
          new_ebuild.ebuild_path, stable_ebuild.ebuild_path, shallow=False)

  # Case where we have the last stable candidate with same version just rev.
  if stable_candidate and stable_candidate.version_no_rev == android_version:
    new_ebuild_path = '%s-r%d.ebuild' % (
        stable_candidate.ebuild_path_no_revision,
        stable_candidate.current_revision + 1)
  else:
    pf = '%s-%s-r1' % (android_pn, android_version)
    new_ebuild_path = os.path.join(package_dir, '%s.ebuild' % pf)

  variables = {'BASE_URL': arc_bucket_url}
  for build, (target, _) in constants.ANDROID_BUILD_TARGETS.iteritems():
    variables[build + '_TARGET'] = '%s-%s' % (build_branch, target)

  portage_util.EBuild.MarkAsStable(
      unstable_ebuild.ebuild_path, new_ebuild_path,
      variables, make_stable=True)
  new_ebuild = portage_util.EBuild(new_ebuild_path)

  # Determine whether this is ebuild is redundant.
  if IsTheNewEBuildRedundant(new_ebuild, stable_candidate):
    msg = 'Previous ebuild with same version found and ebuild is redundant.'
    logging.info(msg)
    os.unlink(new_ebuild_path)
    return None

  if stable_candidate:
    logging.PrintBuildbotLink('Android revisions',
                              GetAndroidRevisionListLink(build_branch,
                                                         stable_candidate,
                                                         new_ebuild))

  git.RunGit(package_dir, ['add', new_ebuild_path])
  if stable_candidate and not stable_candidate.IsSticky():
    git.RunGit(package_dir, ['rm', stable_candidate.ebuild_path])

  # Update ebuild manifest and git add it.
  gen_manifest_cmd = ['ebuild', new_ebuild_path, 'manifest', '--force']
  cros_build_lib.RunCommand(gen_manifest_cmd,
                            extra_env=None, print_cmd=True)
  git.RunGit(package_dir, ['add', 'Manifest'])

  portage_util.EBuild.CommitChange(
      _GIT_COMMIT_MESSAGE % {'android_pn': android_pn,
                             'android_version': android_version},
      package_dir)

  return '%s-%s' % (new_ebuild.package, new_ebuild.version)


def GetParser():
  """Creates the argument parser."""
  parser = commandline.ArgumentParser()
  parser.add_argument('-b', '--boards')
  parser.add_argument('--android_bucket_url',
                      default=constants.ANDROID_BUCKET_URL,
                      type='gs_path')
  parser.add_argument('--android_build_branch',
                      default=constants.ANDROID_BUILD_BRANCH)
  parser.add_argument('--arc_bucket_url',
                      default=constants.ARC_BUCKET_URL,
                      type='gs_path')
  parser.add_argument('-f', '--force_version',
                      help='Android build id to use')
  parser.add_argument('-s', '--srcroot',
                      default=os.path.join(os.environ['HOME'], 'trunk', 'src'),
                      help='Path to the src directory')
  parser.add_argument('-t', '--tracking_branch', default='cros/master',
                      help='Branch we are tracking changes against')
  return parser


def main(argv):
  parser = GetParser()
  options = parser.parse_args(argv)
  options.Freeze()

  overlay_dir = os.path.abspath(_OVERLAY_DIR % {'srcroot': options.srcroot})
  android_package_dir = os.path.join(overlay_dir, constants.ANDROID_CP)
  version_to_uprev = None
  subpaths = None

  (unstable_ebuild, stable_ebuilds) = FindAndroidCandidates(android_package_dir)

  if options.force_version:
    version_to_uprev = options.force_version
    subpaths = IsBuildIdValid(options.android_bucket_url,
                              options.android_build_branch, version_to_uprev)
    if not subpaths:
      logging.error('Requested build %s is not valid' % version_to_uprev)
  else:
    version_to_uprev, subpaths = GetLatestBuild(options.android_bucket_url,
                                                options.android_build_branch)

  acls = MakeAclDict(android_package_dir)
  CopyToArcBucket(options.android_bucket_url, options.android_build_branch,
                  version_to_uprev, subpaths, options.arc_bucket_url, acls)

  stable_candidate = portage_util.BestEBuild(stable_ebuilds)

  if stable_candidate:
    logging.info('Stable candidate found %s' % stable_candidate.version)
  else:
    logging.info('No stable candidate found.')

  tracking_branch = 'remotes/m/%s' % os.path.basename(options.tracking_branch)
  existing_branch = git.GetCurrentBranch(android_package_dir)
  work_branch = cros_mark_as_stable.GitBranch(constants.STABLE_EBUILD_BRANCH,
                                              tracking_branch,
                                              android_package_dir)
  work_branch.CreateBranch()

  # In the case of uprevving overlays that have patches applied to them,
  # include the patched changes in the stabilizing branch.
  if existing_branch:
    git.RunGit(overlay_dir, ['rebase', existing_branch])

  android_version_atom = MarkAndroidEBuildAsStable(
      stable_candidate, unstable_ebuild, constants.ANDROID_PN,
      version_to_uprev, android_package_dir,
      options.android_build_branch, options.arc_bucket_url)
  if android_version_atom:
    if options.boards:
      cros_mark_as_stable.CleanStalePackages(options.srcroot,
                                             options.boards.split(':'),
                                             [android_version_atom])

    # Explicit print to communicate to caller.
    print('ANDROID_VERSION_ATOM=%s' % android_version_atom)
