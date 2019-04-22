# -*- coding: utf-8 -*-
# Copyright 2018 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Sync a git repository to a given manifest.

This script is intended to define all of the ways that a source checkout can be
defined for a Chrome OS builder.

If a sync completes successfully, the checked out code will exactly match
whatever manifest is defined, and no local git branches will remain. Extraneous
files not under version management will be ignored.

It is NOT safe to assume that the checkout can be further updated with
a simple "repo sync", instead you should call this script again with
the same options.
"""

from __future__ import print_function

from chromite.cbuildbot import manifest_version
from chromite.cbuildbot import patch_series
from chromite.cbuildbot import repository
from chromite.lib import commandline
from chromite.lib import cros_logging as logging
from chromite.lib import config_lib
from chromite.lib import gerrit
from chromite.lib import osutils


def GetParser():
  """Creates the argparse parser."""
  parser = commandline.ArgumentParser(description=__doc__)

  parser.add_argument('--repo-root', type='path', default='.',
                      help='Path to the repo root to sync.')

  manifest_group = parser.add_argument_group(
      'Manifest',
      description='What manifest do we sync?')

  manifest_ex = manifest_group.add_mutually_exclusive_group()
  manifest_ex.add_argument(
      '--branch',
      default='master',
      help='Sync to top of given branch.')
  manifest_ex.add_argument(
      '--buildspec',
      help='Path to manifest, relative to manifest-versions root.')
  manifest_ex.add_argument(
      '--version',
      help='Shorthand for an official release buildspec. e.g. 9799.0.0')
  manifest_ex.add_argument(
      '--manifest-file', type='path',
      help='Sync to an existing local manifest file.')

  manifest_url_ex = manifest_group.add_mutually_exclusive_group()
  manifest_url_ex.add_argument(
      '--external', action='store_true', default=False,
      help='Sync to the external version of a manifest. Switch from '
           'manifest-versions-internal to manifest-versions for buildspecs. '
           'Not usable with --manifest.')
  manifest_url_ex.add_argument(
      '--manifest-url', help='Manually set URL to fetch repo manifest from.')

  patch_group = parser.add_argument_group(
      'Patch',
      description='Which patches should be included with the build?')
  patch_group.add_argument(
      '-g', '--gerrit-patches', action='split_extend', default=[],
      metavar='Id1 *int_Id2...IdN',
      help='Space-separated list of short-form Gerrit '
           "Change-Id's or change numbers to patch. "
           "Please prepend '*' to internal Change-Id's")

  resources_group = parser.add_argument_group(
      'Resources',
      description='External resources that might be needed.')

  resources_group.add_argument(
      '--manifest-versions-int', type='path',
      help='Directory for internal manifest versions checkout. '
           'May be refreshed.')

  resources_group.add_argument(
      '--manifest-versions-ext', type='path',
      help='Directory for internal manifest versions checkout. '
           'May be refreshed.')

  optimization_group = parser.add_argument_group(
      'Optimization',
      description='Hints provided to possibly speed up initial sync.')

  optimization_group.add_argument(
      '--copy-repo', type='path',
      help='Path to an existing repo root. Used to preload the local '
           "checkout if the local checkout doesn't exist.")
  optimization_group.add_argument(
      '--git-cache-dir', type='path',
      help='Git cache directory to use.')
  optimization_group.add_argument(
      '--repo-url', help='Repo repository location.')

  return parser


def PrepareManifestVersions(options):
  """Select manifest-versions checkout to use, and update it.

  Looks at command line options to decide which manifest-versions checkout to
  use, and updates (or creates) it as needed.

  Args:
    options: Parsed command line options.

  Returns:
    Full path to manifest-versions directory to use for this sync.

  Raises:
    AssertionError: If the needed manifest-versions path wasn't con the
      command line.
  """
  site_params = config_lib.GetSiteParams()

  if options.external:
    assert options.manifest_versions_ext, '--manifest-versions-ext required.'
    manifest_versions_url = site_params.MANIFEST_VERSIONS_GOB_URL
    manifest_versions_path = options.manifest_versions_ext
  else:
    assert options.manifest_versions_int, '--manifest-versions-int required.'
    manifest_versions_url = site_params.MANIFEST_VERSIONS_INT_GOB_URL
    manifest_versions_path = options.manifest_versions_int

  # Resolve buildspecs against a current manifest versions value.
  manifest_version.RefreshManifestCheckout(
      manifest_versions_path, manifest_versions_url)

  return manifest_versions_path


def ResolveLocalManifestPath(options):
  """Based on command line options, decide what local manifest file to use.

  Args:
    options: Our parsed command line options.

  Returns:
    Path to local manifest file to use, or None for no file.
  """
  if options.manifest_file:
    # If the user gives us an explicit local manifest file, use it.
    return options.manifest_file

  elif options.buildspec:
    # Buildspec builds use a manifest file from manifest_versions. We do NOT
    # use manifest_versions as the manifest git repo, because it's so large that
    # sync time would be a major performance problem.
    manifest_versions_path = PrepareManifestVersions(options)
    return manifest_version.ResolveBuildspec(
        manifest_versions_path, options.buildspec)

  elif options.version:
    # Versions are a short hand version of a buildspec.
    manifest_versions_path = PrepareManifestVersions(options)
    return manifest_version.ResolveBuildspecVersion(
        manifest_versions_path, options.version)

  elif options.branch:
    # Branch checkouts use our normal manifest repos, not a local manifest file.
    return None

  else:
    assert False, 'No sync options specified. Should not be possible.'


def main(argv):
  parser = GetParser()
  options = parser.parse_args(argv)
  options.Freeze()

  local_manifest = ResolveLocalManifestPath(options)

  if local_manifest:
    logging.info('Using local_manifest: %s', local_manifest)

  if options.manifest_url:
    manifest_url = options.manifest_url
  elif options.external:
    manifest_url = config_lib.GetSiteParams().MANIFEST_URL
  else:
    manifest_url = config_lib.GetSiteParams().MANIFEST_INT_URL

  osutils.SafeMakedirs(options.repo_root)
  repo = repository.RepoRepository(
      manifest_repo_url=manifest_url,
      directory=options.repo_root,
      branch=options.branch,
      git_cache_dir=options.git_cache_dir,
      repo_url=options.repo_url)

  if options.copy_repo:
    repo.PreLoad(options.copy_repo)

  if repository.IsARepoRoot(options.repo_root):
    repo.BuildRootGitCleanup(prune_all=True)

  repo.Sync(local_manifest=local_manifest, detach=True)

  if options.gerrit_patches:
    patches = gerrit.GetGerritPatchInfo(options.gerrit_patches)
    # TODO: Extract patches from manifest synced.

    helper_pool = patch_series.HelperPool.SimpleCreate(
        cros_internal=not options.external, cros=True)

    series = patch_series.PatchSeries(
        path=options.repo_root, helper_pool=helper_pool, forced_manifest=None)

    _, failed_tot, failed_inflight = series.Apply(patches)

    failed = failed_tot + failed_inflight
    if failed:
      logging.error('Failed to apply: %s', ', '.join(str(p) for p in failed))
      return 1
