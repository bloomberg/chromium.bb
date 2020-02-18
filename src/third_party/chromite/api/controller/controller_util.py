# -*- coding: utf-8 -*-
# Copyright 2019 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Utility functions that are useful for controllers."""

from __future__ import print_function

from chromite.api.gen.chromiumos import common_pb2
from chromite.cbuildbot import goma_util
from chromite.lib import portage_util
from chromite.lib.build_target_util import BuildTarget
from chromite.lib.chroot_lib import Chroot


class Error(Exception):
  """Base error class for the module."""


class InvalidMessageError(Error):
  """Invalid message."""


def ParseChroot(chroot_message):
  """Create a chroot object from the chroot message.

  Args:
    chroot_message (common_pb2.Chroot): The chroot message.

  Returns:
    Chroot: The parsed chroot object.

  Raises:
    AssertionError: When the message is not a Chroot message.
  """
  assert isinstance(chroot_message, common_pb2.Chroot)

  path = chroot_message.path
  cache_dir = chroot_message.cache_dir
  chrome_root = chroot_message.chrome_dir

  use_flags = [u.flag for u in chroot_message.env.use_flags]
  features = [f.feature for f in chroot_message.env.features]

  env = {}
  if use_flags:
    env['USE'] = ' '.join(use_flags)

  # Make sure it'll use the local source to build chrome when we have it.
  if chrome_root:
    env['CHROME_ORIGIN'] = 'LOCAL_SOURCE'

  # TODO(saklein) Remove the default when fully integrated in recipes.
  env['FEATURES'] = 'separatedebug'
  if features:
    env['FEATURES'] = ' '.join(features)

  goma = None
  if chroot_message.goma.goma_dir:
    chromeos_goma_dir = chroot_message.goma.chromeos_goma_dir or None
    goma_approach = None
    if chroot_message.goma.goma_approach == common_pb2.GomaConfig.RBE_PROD:
      goma_approach = goma_util.GomaApproach('?prod', 'goma.chromium.org', True)
    elif chroot_message.goma.goma_approach == common_pb2.GomaConfig.RBE_STAGING:
      goma_approach = goma_util.GomaApproach('?staging',
                                             'staging-goma.chromium.org', True)
    goma = goma_util.Goma(chroot_message.goma.goma_dir,
                          chroot_message.goma.goma_client_json,
                          stage_name='BuildAPI',
                          chromeos_goma_dir=chromeos_goma_dir,
                          chroot_dir=path,
                          goma_approach=goma_approach)

  return Chroot(path=path, cache_dir=cache_dir, chrome_root=chrome_root,
                env=env, goma=goma)


def ParseBuildTarget(build_target_message):
  """Create a BuildTarget object from a build_target message.

  Args:
    build_target_message (common_pb2.BuildTarget): The BuildTarget message.

  Returns:
    BuildTarget: The parsed instance.

  Raises:
    AssertionError: When the field is not a BuildTarget message.
  """
  assert isinstance(build_target_message, common_pb2.BuildTarget)

  return BuildTarget(build_target_message.name)


def ParseBuildTargets(repeated_build_target_field):
  """Create a BuildTarget for each entry in the repeated field.

  Args:
    repeated_build_target_field: The repeated BuildTarget field.

  Returns:
    list[BuildTarget]: The parsed BuildTargets.

  Raises:
    AssertionError: When the field contains non-BuildTarget messages.
  """
  return [ParseBuildTarget(target) for target in repeated_build_target_field]


def CPVToPackageInfo(cpv, package_info):
  """Helper to translate CPVs into a PackageInfo message."""
  package_info.package_name = cpv.package
  if cpv.category:
    package_info.category = cpv.category
  if cpv.version:
    package_info.version = cpv.version


def PackageInfoToCPV(package_info):
  """Helper to translate a PackageInfo message into a CPV."""
  if not package_info or not package_info.package_name:
    return None

  return portage_util.SplitCPV(PackageInfoToString(package_info), strict=False)


def PackageInfoToString(package_info):
  # Combine the components into the full CPV string that SplitCPV parses.
  # TODO: Turn portage_util.CPV into a class that can handle building out an
  #  instance from components.
  if not package_info.package_name:
    raise ValueError('Invalid package_info.')

  c = ('%s/' % package_info.category) if package_info.category else ''
  p = package_info.package_name
  v = ('-%s' % package_info.version) if package_info.version else ''
  return '%s%s%s' % (c, p, v)


def CPVToString(cpv):
  """Get the most useful string representation from a CPV.

  Args:
    cpv (portage_util.CPV): The CPV object.

  Returns:
    str

  Raises:
    ValueError - when the CPV has no useful fields set.
  """
  if cpv.cpf:
    return cpv.cpf
  elif cpv.cpv:
    return cpv.cpv
  elif cpv.cp:
    return cpv.cp
  elif cpv.package:
    return cpv.package
  else:
    raise ValueError('Invalid CPV provided.')
