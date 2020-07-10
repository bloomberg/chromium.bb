# -*- coding: utf-8 -*-
# Copyright 2017 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Build stages specific to firmware builds.

Firmware builds use a mix of standard stages, and custom stages
related to how build artifacts are generated and published.
"""

from __future__ import print_function

import datetime
import json
import os
import shutil

from chromite.cbuildbot import commands
from chromite.cbuildbot.stages import generic_stages
from chromite.cbuildbot.stages import workspace_stages
from chromite.lib import config_lib
from chromite.lib import constants
from chromite.lib import cros_build_lib
from chromite.lib import cros_logging as logging
from chromite.lib import gs
from chromite.lib import osutils
from chromite.lib import portage_util
from chromite.lib import timeout_util


class UnsafeBuildForPushImage(Exception):
  """Raised if push_image is run against a non-signable build."""


class WorkspaceArchiveBase(workspace_stages.WorkspaceStageBase,
                           generic_stages.BoardSpecificBuilderStage,
                           generic_stages.ArchivingStageMixin):
  """Base class for workspace archive stages.

  The expectation is that the archive stages will be creating a "dummy" upload
  that looks like an older style branched infrastructure build would have
  generated in addtion to a firmwarebranch set of archive results.
  """
  DUMMY_NAME = 'dummy'

  @property
  def dummy_config(self):
    """Uniqify the name across boards."""
    if self._run.options.debug:
      return '%s-%s-tryjob' % (self._current_board, self.DUMMY_NAME)
    else:
      return '%s-%s' % (self._current_board, self.DUMMY_NAME)

  @property
  def dummy_version(self):
    """Uniqify the name across boards."""
    workspace_version_info = self.GetWorkspaceVersionInfo()

    if self._run.options.debug:
      build_identifier, _ = self._run.GetCIDBHandle()
      build_id = build_identifier.cidb_id
      return 'R%s-%s-b%s' % (
          workspace_version_info.chrome_branch,
          workspace_version_info.VersionString(),
          build_id)
    else:
      return 'R%s-%s' % (
          workspace_version_info.chrome_branch,
          workspace_version_info.VersionString())

  @property
  def dummy_archive_url(self):
    """Uniqify the name across boards."""
    return self.UniqifyArchiveUrl(config_lib.GetSiteParams().ARCHIVE_URL)

  def UniqifyArchiveUrl(self, archive_url):
    """Return an archive url unique to the current board.

    Args:
      archive_url: The base archive URL (e.g. 'chromeos-image-archive').

    Returns:
      The unique archive URL.
    """
    return os.path.join(archive_url, self.dummy_config, self.dummy_version)

  def GetDummyArchiveUrls(self):
    """Returns upload URLs for dummy artifacts based on artifacts.json."""
    upload_urls = [self.dummy_archive_url]
    artifacts_file = portage_util.ReadOverlayFile(
        'scripts/artifacts.json',
        board=self._current_board,
        buildroot=self._build_root)
    if artifacts_file is not None:
      artifacts_json = json.loads(artifacts_file)
      extra_upload_urls = artifacts_json.get('extra_upload_urls', [])
      upload_urls += [self.UniqifyArchiveUrl(url) for url in extra_upload_urls]
    return upload_urls

  def UploadDummyArtifact(self, path, faft_hack=False):
    """Upload artifacts to the dummy build results."""
    logging.info('UploadDummyArtifact: %s', path)
    with osutils.TempDir(prefix='dummy') as tempdir:
      artifact_path = os.path.join(
          tempdir,
          '%s/%s' % (self._current_board, os.path.basename(path)))

      logging.info('Rename: %s -> %s', path, artifact_path)
      os.mkdir(os.path.join(tempdir, self._current_board))
      shutil.copyfile(path, artifact_path)

      logging.info('Main artifact from: %s', artifact_path)

      if faft_hack:
        # We put the firmware artifact in a directory named by board so that
        # immutable FAFT infrastructure can find it. We should remove this.
        self.UploadArtifact(
            artifact_path, archive=True, prefix=self._current_board)
      else:
        self.UploadArtifact(artifact_path, archive=True)

    gs_context = gs.GSContext(dry_run=self._run.options.debug_forced)
    for url in self.GetDummyArchiveUrls():
      logging.info('Uploading dummy artifact to %s...', url)
      with timeout_util.Timeout(20 * 60):
        logging.info('Dummy artifact from: %s', path)
        gs_context.CopyInto(path, url, parallel=True, recursive=True)

  def PushBoardImage(self):
    """Helper method to run push_image against the dummy boards artifacts."""
    # This helper script is only available on internal manifests currently.
    if not self._run.config['internal']:
      raise UnsafeBuildForPushImage("Can't use push_image on external builds.")

    logging.info('Use pushimage to publish signing artifacts for: %s',
                 self._current_board)

    # Push build artifacts to gs://chromeos-releases for signing and release.
    # This runs TOT pushimage against the build artifacts for the branch.
    commands.PushImages(
        board=self._current_board,
        archive_url=self.dummy_archive_url,
        dryrun=self._run.options.debug or not self._run.config['push_image'],
        profile=self._run.options.profile or self._run.config['profile'],
        sign_types=self._run.config['sign_types'] or [],
        buildroot=self._build_root)

  def CreateDummyMetadataJson(self):
    """Create/publish the firmware build artifact for the current board."""
    workspace_version_info = self.GetWorkspaceVersionInfo()

    # Use the metadata for the main build, with selected fields modified.
    board_metadata = self._run.attrs.metadata.GetDict()
    board_metadata['boards'] = [self._current_board]
    board_metadata['branch'] = self._run.config.workspace_branch
    board_metadata['version_full'] = self.dummy_version
    board_metadata['version_milestone'] = \
        workspace_version_info.chrome_branch
    board_metadata['version_platform'] = \
        workspace_version_info.VersionString()
    board_metadata['version'] = {
        'platform': workspace_version_info.VersionString(),
        'full': self.dummy_version,
        'milestone': workspace_version_info.chrome_branch,
    }

    current_time = datetime.datetime.now()
    current_time_stamp = cros_build_lib.UserDateTimeFormat(timeval=current_time)

    # We report the build as passing, since we can't get here if isn't.
    board_metadata['status'] = {
        'status': 'pass',
        'summary': '',
        'current-time': current_time_stamp,
    }

    with osutils.TempDir(prefix='metadata') as tempdir:
      metadata_path = os.path.join(tempdir, constants.METADATA_JSON)
      logging.info('Writing metadata to %s.', metadata_path)
      osutils.WriteFile(metadata_path,
                        json.dumps(board_metadata, indent=2, sort_keys=True),
                        atomic=True)

      self.UploadDummyArtifact(metadata_path)


class FirmwareArchiveStage(WorkspaceArchiveBase):
  """Generates and publishes firmware specific build artifacts.

  This stage publishes <board>/firmware_from_source.tar.bz2 to this
  builds standard build artifacts, and also generates a 'fake' build
  result (called a Dummy result) that looks like it came from a
  traditional style firmware builder for a single board on the
  firmware branch.

  The secondary result only contains firmware_from_source.tar.bz2 and
  a dummy version of metadata.json.
  """
  DUMMY_NAME = 'firmware'

  def CreateFirmwareArchive(self):
    """Create/publish the firmware build artifact for the current board."""
    with osutils.TempDir(prefix='metadata') as tempdir:
      firmware_path = os.path.join(tempdir, constants.FIRMWARE_ARCHIVE_NAME)

      logging.info('Create %s', firmware_path)

      commands.BuildFirmwareArchive(
          self._build_root, self._current_board,
          self.archive_path, firmware_path)

      self.UploadDummyArtifact(firmware_path, faft_hack=True)

  def PerformStage(self):
    """Archive and publish the firmware build artifacts."""
    logging.info('Firmware board: %s', self._current_board)
    logging.info('Firmware version: %s', self.dummy_version)
    logging.info('Archive build as: %s', self.dummy_config)

    # Link dummy build artifacts from build.
    dummy_http_url = gs.GsUrlToHttp(self.dummy_archive_url,
                                    public=False, directory=True)

    label = '%s firmware [%s]' % (self._current_board, self.dummy_version)
    logging.PrintBuildbotLink(label, dummy_http_url)

    # Upload all artifacts.
    self.CreateFirmwareArchive()
    self.CreateDummyMetadataJson()
    self.PushBoardImage()


class FactoryArchiveStage(WorkspaceArchiveBase):
  """Generates and publishes factory specific build artifacts."""

  DUMMY_NAME = 'factory'

  def CreateFactoryZip(self):
    """Create/publish the firmware build artifact for the current board."""
    logging.info('Create factory_image.zip')

    # TODO: Move this image creation logic back into WorkspaceBuildImages.

    factory_install_symlink = None
    if 'factory_install' in self._run.config['images']:
      alias = commands.BuildFactoryInstallImage(
          self._build_root,
          self._current_board,
          extra_env=self._portage_extra_env)

      factory_install_symlink = self.GetImageDirSymlink(alias, self._build_root)
      if self._run.config['factory_install_netboot']:
        commands.MakeNetboot(
            self._build_root,
            self._current_board,
            factory_install_symlink)

    # Build and upload factory zip if needed.
    assert self._run.config['factory_toolkit']

    with osutils.TempDir(prefix='factory_zip') as zip_dir:
      filename = commands.BuildFactoryZip(
          self._build_root,
          self._current_board,
          zip_dir,
          factory_install_symlink,
          self.dummy_version)

      self.UploadDummyArtifact(os.path.join(zip_dir, filename))

  def CreateTestImageTar(self):
    """Create and upload chromiumos_test_image.tar.xz.

    This depends on the WorkspaceBuildImage stage having previously created
    chromiumos_test_image.bin.
    """
    with osutils.TempDir(prefix='test_image_dir') as tempdir:
      target = os.path.join(tempdir, constants.TEST_IMAGE_TAR)

      cros_build_lib.CreateTarball(
          target,
          inputs=[constants.TEST_IMAGE_BIN],
          cwd=self.GetImageDirSymlink(pointer='latest',
                                      buildroot=self._build_root),
          compression=cros_build_lib.COMP_GZIP)

      self.UploadDummyArtifact(target)

  def PerformStage(self):
    """Archive and publish the factory build artifacts."""
    logging.info('Factory version: %s', self.dummy_version)
    logging.info('Archive build as: %s', self.dummy_config)

    # Link dummy build artifacts from build.
    dummy_http_url = gs.GsUrlToHttp(self.dummy_archive_url,
                                    public=False, directory=True)

    label = '%s factory [%s]' % (self._current_board, self.dummy_version)
    logging.PrintBuildbotLink(label, dummy_http_url)

    # factory_image.zip
    self.CreateFactoryZip()
    self.CreateTestImageTar()
    self.CreateDummyMetadataJson()
    self.PushBoardImage()
