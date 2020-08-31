# -*- coding: utf-8 -*-
# Copyright 2019 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Sysroot controller."""

from __future__ import print_function

import os
import sys

from chromite.api import controller
from chromite.api import faux
from chromite.api import validate
from chromite.api.controller import controller_util
from chromite.api.metrics import deserialize_metrics_log
from chromite.lib import cros_build_lib
from chromite.lib import cros_logging as logging
from chromite.lib import goma_lib
from chromite.lib import osutils
from chromite.lib import portage_util
from chromite.lib import sysroot_lib
from chromite.service import sysroot
from chromite.utils import metrics


assert sys.version_info >= (3, 6), 'This module requires Python 3.6+'


_ACCEPTED_LICENSES = '@CHROMEOS'


@faux.all_empty
@validate.require('build_target.name')
@validate.validation_complete
def Create(input_proto, output_proto, _config):
  """Create or replace a sysroot."""
  update_chroot = not input_proto.flags.chroot_current
  replace_sysroot = input_proto.flags.replace

  build_target = controller_util.ParseBuildTarget(input_proto.build_target,
                                                  input_proto.profile)
  run_configs = sysroot.SetupBoardRunConfig(
      force=replace_sysroot, upgrade_chroot=update_chroot)

  try:
    created = sysroot.Create(build_target, run_configs,
                             accept_licenses=_ACCEPTED_LICENSES)
  except sysroot.Error as e:
    cros_build_lib.Die(e)

  output_proto.sysroot.path = created.path
  output_proto.sysroot.build_target.name = build_target.name

  return controller.RETURN_CODE_SUCCESS


@faux.all_empty
@validate.require('build_target.name')
@validate.validation_complete
def CreateSimpleChromeSysroot(input_proto, output_proto, _config):
  """Create a sysroot for SimpleChrome to use."""
  build_target_name = input_proto.build_target.name
  use_flags = input_proto.use_flags

  sysroot_tar_path = sysroot.CreateSimpleChromeSysroot(build_target_name,
                                                       use_flags)
  # By assigning this Path variable to the tar path, the tar file will be
  # copied out to the input_proto's ResultPath location.
  output_proto.sysroot_archive.path = sysroot_tar_path

  return controller.RETURN_CODE_SUCCESS


@faux.all_empty
@validate.require('build_target.name', 'packages')
@validate.validation_complete
def GenerateArchive(input_proto, output_proto, _config):
  """Generate a sysroot. Typically used by informational builders."""
  build_target_name = input_proto.build_target.name
  pkg_list = []
  for package in input_proto.packages:
    pkg_list.append('%s/%s' % (package.category, package.package_name))

  with osutils.TempDir(delete=False) as temp_output_dir:
    sysroot_tar_path = sysroot.GenerateArchive(temp_output_dir,
                                               build_target_name,
                                               pkg_list)

  # By assigning this Path variable to the tar path, the tar file will be
  # copied out to the input_proto's ResultPath location.
  output_proto.sysroot_archive.path = sysroot_tar_path


def _MockFailedPackagesResponse(_input_proto, output_proto, _config):
  """Mock error response that populates failed packages."""
  pkg = output_proto.failed_packages.add()
  pkg.package_name = 'package'
  pkg.category = 'category'
  pkg.version = '1.0.0_rc-r1'

  pkg2 = output_proto.failed_packages.add()
  pkg2.package_name = 'bar'
  pkg2.category = 'foo'
  pkg2.version = '3.7-r99'


@faux.empty_success
@faux.error(_MockFailedPackagesResponse)
@validate.require('sysroot.path', 'sysroot.build_target.name')
@validate.exists('sysroot.path')
@validate.validation_complete
def InstallToolchain(input_proto, output_proto, _config):
  """Install the toolchain into a sysroot."""
  compile_source = (
      input_proto.flags.compile_source or input_proto.flags.toolchain_changed)

  sysroot_path = input_proto.sysroot.path

  build_target = controller_util.ParseBuildTarget(
      input_proto.sysroot.build_target)
  target_sysroot = sysroot_lib.Sysroot(sysroot_path)
  run_configs = sysroot.SetupBoardRunConfig(usepkg=not compile_source)

  _LogBinhost(build_target.name)

  try:
    sysroot.InstallToolchain(build_target, target_sysroot, run_configs)
  except sysroot_lib.ToolchainInstallError as e:
    # Error installing - populate the failed package info.
    for package in e.failed_toolchain_info:
      package_info = output_proto.failed_packages.add()
      controller_util.CPVToPackageInfo(package, package_info)

    return controller.RETURN_CODE_UNSUCCESSFUL_RESPONSE_AVAILABLE

  return controller.RETURN_CODE_SUCCESS


@faux.empty_success
@faux.error(_MockFailedPackagesResponse)
@validate.require('sysroot.build_target.name')
@validate.exists('sysroot.path')
@validate.validation_complete
@metrics.collect_metrics
def InstallPackages(input_proto, output_proto, _config):
  """Install packages into a sysroot, building as necessary and permitted."""
  compile_source = (
      input_proto.flags.compile_source or input_proto.flags.toolchain_changed)
  # A new toolchain version will not yet have goma support, so goma must be
  # disabled when we are testing toolchain changes.
  use_goma = (
      input_proto.flags.use_goma and not input_proto.flags.toolchain_changed)

  target_sysroot = sysroot_lib.Sysroot(input_proto.sysroot.path)
  build_target = controller_util.ParseBuildTarget(
      input_proto.sysroot.build_target)
  packages = [controller_util.PackageInfoToString(x)
              for x in input_proto.packages]

  if not target_sysroot.IsToolchainInstalled():
    cros_build_lib.Die('Toolchain must first be installed.')

  _LogBinhost(build_target.name)

  use_flags = [u.flag for u in input_proto.use_flags]
  build_packages_config = sysroot.BuildPackagesRunConfig(
      usepkg=not compile_source,
      install_debug_symbols=True,
      packages=packages,
      use_flags=use_flags,
      use_goma=use_goma,
      incremental_build=False)

  try:
    sysroot.BuildPackages(build_target, target_sysroot, build_packages_config)
  except sysroot_lib.PackageInstallError as e:
    if not e.failed_packages:
      # No packages to report, so just exit with an error code.
      return controller.RETURN_CODE_COMPLETED_UNSUCCESSFULLY

    # We need to report the failed packages.
    for package in e.failed_packages:
      package_info = output_proto.failed_packages.add()
      controller_util.CPVToPackageInfo(package, package_info)

    return controller.RETURN_CODE_UNSUCCESSFUL_RESPONSE_AVAILABLE

  # Copy goma logs to specified directory if there is a goma_config and
  # it contains a log_dir to store artifacts.
  if input_proto.goma_config.log_dir.dir:
    # Get the goma log directory based on the GLOG_log_dir env variable.
    # TODO(crbug.com/1045001): Replace environment variable with query to
    # goma object after goma refactoring allows this.
    log_source_dir = os.getenv('GLOG_log_dir')
    if not log_source_dir:
      cros_build_lib.Die('GLOG_log_dir must be defined.')
    archiver = goma_lib.LogsArchiver(
        log_source_dir,
        dest_dir=input_proto.goma_config.log_dir.dir,
        stats_file=input_proto.goma_config.stats_file,
        counterz_file=input_proto.goma_config.counterz_file)
    archiver_tuple = archiver.Archive()
    if archiver_tuple.stats_file:
      output_proto.goma_artifacts.stats_file = archiver_tuple.stats_file
    if archiver_tuple.counterz_file:
      output_proto.goma_artifacts.counterz_file = archiver_tuple.counterz_file
    output_proto.goma_artifacts.log_files[:] = archiver_tuple.log_files

  # Read metric events log and pipe them into output_proto.events.
  deserialize_metrics_log(output_proto.events, prefix=build_target.name)


def _LogBinhost(board):
  """Log the portage binhost for the given board."""
  binhost = portage_util.PortageqEnvvar('PORTAGE_BINHOST', board=board,
                                        allow_undefined=True)
  if not binhost:
    logging.warning('Portage Binhost not found.')
  else:
    logging.info('Portage Binhost: %s', binhost)
