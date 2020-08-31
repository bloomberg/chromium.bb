# -*- coding: utf-8 -*-
# Copyright 2016 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""This module uprevs Android for cbuildbot.

After calling, it prints outs ANDROID_VERSION_ATOM=(version atom string).  A
caller could then use this atom with emerge to build the newly uprevved version
of Android e.g.

./cros_mark_android_as_stable \
    --android_build_branch=git_pi-arc \
    --android_package=android-container-pi

Returns chromeos-base/android-container-pi-6417892-r1

emerge-eve =chromeos-base/android-container-pi-6417892-r1
"""

from __future__ import print_function

import filecmp
import glob
import os
import re
import time
import sys

from chromite.lib import constants
from chromite.lib import commandline
from chromite.lib import cros_build_lib
from chromite.lib import cros_logging as logging
from chromite.lib import git
from chromite.lib import gs
from chromite.lib import portage_util
from chromite.scripts import cros_mark_as_stable


assert sys.version_info >= (3, 6), 'This module requires Python 3.6+'


# Dir where all the action happens.
_OVERLAY_DIR = '%(srcroot)s/private-overlays/project-cheets-private/'

_GIT_COMMIT_MESSAGE = """Marking latest for %(android_package)s ebuild with \
version %(android_version)s as stable.

BUG=None
TEST=CQ
"""


def IsBuildIdValid(bucket_url, build_branch, build_id, targets):
  """Checks that a specific build_id is valid.

  Looks for that build_id for all builds. Confirms that the subpath can
  be found and that the zip file is present in that subdirectory.

  Args:
    bucket_url: URL of Android build gs bucket
    build_branch: branch of Android builds
    build_id: A string. The Android build id number to check.
    targets: Dict from build key to (targe build suffix, artifact file pattern)
        pair.

  Returns:
    Returns subpaths dictionary if build_id is valid.
    None if the build_id is not valid.
  """
  gs_context = gs.GSContext()
  subpaths_dict = {}
  for build, (target, _) in targets.items():
    build_dir = '%s-%s' % (build_branch, target)
    build_id_path = os.path.join(bucket_url, build_dir, build_id)

    # Find name of subpath.
    try:
      subpaths = gs_context.List(build_id_path)
    except gs.GSNoSuchKey:
      logging.warning(
          'Directory [%s] does not contain any subpath, ignoring it.',
          build_id_path)
      return None
    if len(subpaths) > 1:
      logging.warning(
          'Directory [%s] contains more than one subpath, ignoring it.',
          build_id_path)
      return None

    subpath_dir = subpaths[0].url.rstrip('/')
    subpath_name = os.path.basename(subpath_dir)

    # Look for a zipfile ending in the build_id number.
    try:
      gs_context.List(subpath_dir)
    except gs.GSNoSuchKey:
      logging.warning(
          'Did not find a file for build id [%s] in directory [%s].',
          build_id, subpath_dir)
      return None

    # Record subpath for the build.
    subpaths_dict[build] = subpath_name

  # If we got here, it means we found an appropriate build for all platforms.
  return subpaths_dict


def GetLatestBuild(bucket_url, build_branch, targets):
  """Searches the gs bucket for the latest green build.

  Args:
    bucket_url: URL of Android build gs bucket
    build_branch: branch of Android builds
    targets: Dict from build key to (targe build suffix, artifact file pattern)
        pair.

  Returns:
    Tuple of (latest version string, subpaths dictionary)
    If no latest build can be found, returns None, None
  """
  gs_context = gs.GSContext()
  common_build_ids = None
  # Find builds for each target.
  for target, _ in targets.values():
    build_dir = '-'.join((build_branch, target))
    base_path = os.path.join(bucket_url, build_dir)
    build_ids = []
    for gs_result in gs_context.List(base_path):
      # Remove trailing slashes and get the base name, which is the build_id.
      build_id = os.path.basename(gs_result.url.rstrip('/'))
      if not build_id.isdigit():
        logging.warning('Directory [%s] does not look like a valid build_id.',
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
    logging.warning('Did not find a build_id common to all platforms.')
    return None, None

  # Otherwise, find the most recent one that is valid.
  for build_id in sorted(common_build_ids, key=int, reverse=True):
    subpaths = IsBuildIdValid(bucket_url, build_branch, build_id, targets)
    if subpaths:
      return build_id, subpaths

  # If not found, no build_id is valid.
  logging.warning('Did not find a build_id valid on all platforms.')
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
    logging.warning('Missing stable ebuild for %s', package_dir)

  return portage_util.BestEBuild(unstable_ebuilds), stable_ebuilds


def _GetArcBasename(build, basename):
  """Tweaks filenames between Android bucket and ARC++ bucket.

  Android builders create build artifacts with the same name for -user and
  -userdebug builds, which breaks the android-container ebuild (b/33072485).
  When copying the artifacts from the Android bucket to the ARC++ bucket some
  artifacts will be renamed from the usual pattern
  *cheets_${ARCH}-target_files-S{VERSION}.zip to
  cheets_${BUILD_NAME}-target_files-S{VERSION}.zip which will typically look
  like cheets_(${LABEL})*${ARCH}_userdebug-target_files-S{VERSION}.zip.

  Args:
    build: the build being mirrored, e.g. 'X86', 'ARM', 'X86_USERDEBUG'.
    basename: the basename of the artifact to copy.

  Returns:
    The basename of the destination.
  """
  if build not in constants.ARC_BUILDS_NEED_ARTIFACTS_RENAMED:
    return basename
  if basename in constants.ARC_ARTIFACTS_RENAME_NOT_NEEDED:
    return basename
  to_discard, sep, to_keep = basename.partition('-')
  if not sep:
    logging.error(('Build %s: Could not find separator "-" in artifact'
                   ' basename %s'), build, basename)
    return basename
  if 'cheets_' in to_discard:
    return 'cheets_%s-%s' % (build.lower(), to_keep)
  elif 'bertha_' in to_discard:
    return 'bertha_%s-%s' % (build.lower(), to_keep)
  logging.error('Build %s: Unexpected artifact basename %s',
                build, basename)
  return basename


def CopyToArcBucket(android_bucket_url, build_branch, build_id, subpaths,
                    targets, arc_bucket_url, acls):
  """Copies from source Android bucket to ARC++ specific bucket.

  Copies each build to the ARC bucket eliminating the subpath.
  Applies build specific ACLs for each file.

  Args:
    android_bucket_url: URL of Android build gs bucket
    build_branch: branch of Android builds
    build_id: A string. The Android build id number to check.
    subpaths: Subpath dictionary for each build to copy.
    targets: Dict from build key to (targe build suffix, artifact file pattern)
        pair.
    arc_bucket_url: URL of the target ARC build gs bucket
    acls: ACLs dictionary for each build to copy.
  """
  gs_context = gs.GSContext()
  for build, subpath in subpaths.items():
    target, pattern = targets[build]
    build_dir = '%s-%s' % (build_branch, target)
    android_dir = os.path.join(android_bucket_url, build_dir, build_id, subpath)
    arc_dir = os.path.join(arc_bucket_url, build_dir, build_id)

    # Copy all target files from android_dir to arc_dir, setting ACLs.
    for targetfile in gs_context.List(android_dir):
      if re.search(pattern, targetfile.url):
        basename = os.path.basename(targetfile.url)
        arc_path = os.path.join(arc_dir, _GetArcBasename(build, basename))
        acl = acls[build]
        needs_copy = True
        retry_count = 2

        # Retry in case race condition when several boards trying to copy the
        # same resource
        while True:
          # Check a pre-existing file with the original source.
          if gs_context.Exists(arc_path):
            if (gs_context.Stat(targetfile.url).hash_crc32c !=
                gs_context.Stat(arc_path).hash_crc32c):
              logging.warning('Removing incorrect file %s', arc_path)
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
            try:
              gs_context.Copy(targetfile.url, arc_path, version=0)
            except gs.GSContextPreconditionFailed as error:
              if not retry_count:
                raise error
              # Retry one more time after a short delay
              logging.warning('Will retry copying %s -> %s',
                              targetfile.url, arc_path)
              time.sleep(5)
              retry_count = retry_count - 1
              continue
          gs_context.ChangeACL(arc_path, acl_args_file=acl)
          break


def MirrorArtifacts(android_bucket_url, android_build_branch, arc_bucket_url,
                    acls, targets, version=None):
  """Mirrors artifacts from Android bucket to ARC bucket.

  First, this function identifies which build version should be copied,
  if not given. Please see GetLatestBuild() and IsBuildIdValid() for details.

  On build version identified, then copies target artifacts to the ARC bucket,
  with setting ACLs.

  Args:
    android_bucket_url: URL of Android build gs bucket
    android_build_branch: branch of Android builds
    arc_bucket_url: URL of the target ARC build gs bucket
    acls: ACLs dictionary for each build to copy.
    targets: Dict from build key to (targe build suffix, artifact file pattern)
        pair.
    version: (optional) A string. The Android build id number to check.
        If not passed, detect latest good build version.

  Returns:
    Mirrored version.
  """
  if version:
    subpaths = IsBuildIdValid(
        android_bucket_url, android_build_branch, version, targets)
    if not subpaths:
      logging.error('Requested build %s is not valid', version)
  else:
    version, subpaths = GetLatestBuild(
        android_bucket_url, android_build_branch, targets)

  CopyToArcBucket(android_bucket_url, android_build_branch, version, subpaths,
                  targets, arc_bucket_url, acls)

  return version


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


def MakeBuildTargetDict(package_name, build_branch):
  """Creates a dictionary of build targets.

  Not all targets are common between branches, for example
  sdk_google_cheets_x86 only exists on N.
  This generates a dictionary listing the available build targets for a
  specific branch.

  Args:
    package_name: package name of chromeos arc package.
    build_branch: branch of Android builds.

  Returns:
    Returns build target dictionary.

  Raises:
    ValueError: if the Android build branch is invalid.
  """
  if constants.ANDROID_CONTAINER_PACKAGE_KEYWORD in package_name:
    target_list = {
        constants.ANDROID_MST_BUILD_BRANCH:
        constants.ANDROID_MST_BUILD_TARGETS,
        constants.ANDROID_PI_BUILD_BRANCH:
        constants.ANDROID_PI_BUILD_TARGETS,
        constants.ANDROID_QT_BUILD_BRANCH:
        constants.ANDROID_QT_BUILD_TARGETS,
        constants.ANDROID_RVC_BUILD_BRANCH:
        constants.ANDROID_RVC_BUILD_TARGETS,
    }
  elif constants.ANDROID_VM_PACKAGE_KEYWORD in package_name:
    target_list = {
        constants.ANDROID_VMPI_BUILD_BRANCH:
        constants.ANDROID_VMPI_BUILD_TARGETS,
        constants.ANDROID_VMMST_BUILD_BRANCH:
        constants.ANDROID_VMMST_BUILD_TARGETS,
        constants.ANDROID_VMRVC_BUILD_BRANCH:
        constants.ANDROID_VMRVC_BUILD_TARGETS,
    }
  else:
    raise ValueError('Unknown package: %s' % package_name)
  target = target_list.get(build_branch)
  if not target:
    raise ValueError('Unknown branch: %s' % build_branch)
  return target


def PrintUprevMetadata(build_branch, stable_candidate, new_ebuild):
  """Shows metadata on buildbot page at UprevAndroid step.

  Args:
    build_branch: The branch of Android builds.
    stable_candidate: The existing stable ebuild.
    new_ebuild: The newly written ebuild.
  """
  # Examples:
  # "android-container-pi revved 6461825-r1 -> 6468247-r1"
  # "android-container-pi revved 6461825-r1 -> 6461825-r2 (ebuild update only)"
  msg = '%s revved %s -> %s' % (stable_candidate.pkgname,
                                stable_candidate.version,
                                new_ebuild.version)

  old_android = stable_candidate.version_no_rev
  new_android = new_ebuild.version_no_rev

  if old_android == new_android:
    msg += ' (ebuild update only)'
  else:
    ab_link = ('https://android-build.googleplex.com'
               '/builds/%s/branches/%s/cls?end=%s'
               % (new_android, build_branch, old_android))
    logging.PrintBuildbotLink('Android changelog', ab_link)

  logging.PrintBuildbotStepText(msg)


def MarkAndroidEBuildAsStable(stable_candidate, unstable_ebuild,
                              android_package, android_version, package_dir,
                              build_branch, arc_bucket_url, build_targets):
  r"""Uprevs the Android ebuild.

  This is the main function that uprevs from a stable candidate
  to its new version.

  Args:
    stable_candidate: ebuild that corresponds to the stable ebuild we are
      revving from.  If None, builds the a new ebuild given the version
      with revision set to 1.
    unstable_ebuild: ebuild corresponding to the unstable ebuild for Android.
    android_package: android package name.
    android_version: The \d+ build id of Android.
    package_dir: Path to the android-container package dir.
    build_branch: branch of Android builds.
    arc_bucket_url: URL of the target ARC build gs bucket.
    build_targets: build targets for this particular Android branch.

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
    pf = '%s-%s-r1' % (android_package, android_version)
    new_ebuild_path = os.path.join(package_dir, '%s.ebuild' % pf)

  variables = {'BASE_URL': arc_bucket_url}
  for build, (target, _) in build_targets.items():
    variables[build + '_TARGET'] = '%s-%s' % (build_branch, target)

  portage_util.EBuild.MarkAsStable(
      unstable_ebuild.ebuild_path, new_ebuild_path,
      variables, make_stable=True)
  new_ebuild = portage_util.EBuild(new_ebuild_path)

  # Determine whether this is ebuild is redundant.
  if IsTheNewEBuildRedundant(new_ebuild, stable_candidate):
    msg = 'Previous ebuild with same version found and ebuild is redundant.'
    logging.info(msg)
    logging.PrintBuildbotStepText('%s %s not revved'
                                  % (stable_candidate.pkgname,
                                     stable_candidate.version))
    os.unlink(new_ebuild_path)
    return None

  # PFQ runs should always be able to find a stable candidate.
  if stable_candidate:
    PrintUprevMetadata(build_branch, stable_candidate, new_ebuild)

  git.RunGit(package_dir, ['add', new_ebuild_path])
  if stable_candidate and not stable_candidate.IsSticky():
    git.RunGit(package_dir, ['rm', stable_candidate.ebuild_path])

  # Update ebuild manifest and git add it.
  gen_manifest_cmd = ['ebuild', new_ebuild_path, 'manifest', '--force']
  cros_build_lib.run(gen_manifest_cmd, extra_env=None, print_cmd=True)
  git.RunGit(package_dir, ['add', 'Manifest'])

  portage_util.EBuild.CommitChange(
      _GIT_COMMIT_MESSAGE % {'android_package': android_package,
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
                      required=True,
                      help='Android branch to import from. '
                           'Ex: git_mnc-dr-arc-dev')
  parser.add_argument('--android_package',
                      default=constants.ANDROID_PACKAGE_NAME)
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
  logging.EnableBuildbotMarkers()
  parser = GetParser()
  options = parser.parse_args(argv)
  options.Freeze()

  overlay_dir = os.path.abspath(_OVERLAY_DIR % {'srcroot': options.srcroot})
  android_package_dir = os.path.join(
      overlay_dir,
      portage_util.GetFullAndroidPortagePackageName(options.android_package))
  version_to_uprev = None

  (unstable_ebuild, stable_ebuilds) = FindAndroidCandidates(android_package_dir)
  acls = MakeAclDict(android_package_dir)
  build_targets = MakeBuildTargetDict(options.android_package,
                                      options.android_build_branch)
  # Mirror artifacts, i.e., images and some sdk tools (e.g., adb, aapt).
  version_to_uprev = MirrorArtifacts(options.android_bucket_url,
                                     options.android_build_branch,
                                     options.arc_bucket_url, acls,
                                     build_targets,
                                     options.force_version)

  stable_candidate = portage_util.BestEBuild(stable_ebuilds)

  if stable_candidate:
    logging.info('Stable candidate found %s', stable_candidate.version)
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
      stable_candidate, unstable_ebuild, options.android_package,
      version_to_uprev, android_package_dir,
      options.android_build_branch, options.arc_bucket_url, build_targets)
  if android_version_atom:
    if options.boards:
      cros_mark_as_stable.CleanStalePackages(options.srcroot,
                                             options.boards.split(':'),
                                             [android_version_atom])

    # Explicit print to communicate to caller.
    print('ANDROID_VERSION_ATOM=%s' % android_version_atom)
