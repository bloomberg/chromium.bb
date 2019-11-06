# -*- coding: utf-8 -*-
# Copyright 2019 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Android operations."""

from __future__ import print_function

import os

from chromite.api import validate
from chromite.api.gen.chromite.api import android_pb2
# TODO(crbug/904939): implement service/android.
from chromite.cbuildbot import commands
from chromite.lib import constants
from chromite.lib import osutils
from chromite.lib import portage_util


ANDROIDPIN_MASK_PATH = os.path.join(constants.SOURCE_ROOT,
                                    constants.CHROMIUMOS_OVERLAY_DIR,
                                    'profiles', 'default', 'linux',
                                    'package.mask', 'androidpin')


@validate.require('tracking_branch', 'package_name', 'android_build_branch')
def MarkStable(input_proto, output_proto):
  """Uprev Android, if able.

  Uprev Android, verify that the newly uprevved package can be emerged, and
  return the new package info.

  See AndroidService documentation in api/proto/android.proto.

  Args:
    input_proto (MarkStableRequest): The input proto.
    output_proto (MarkStableReSponse): The output proto.
  """
  tracking_branch = input_proto.tracking_branch
  package_name = input_proto.package_name
  android_build_branch = input_proto.android_build_branch
  android_version = input_proto.android_version
  android_gts_build_branch = input_proto.android_gts_build_branch
  boards = input_proto.boards
  buildroot = input_proto.buildroot

  # Assume success.
  output_proto.status = android_pb2.MARK_STABLE_STATUS_SUCCESS
  # TODO(crbug/904939): This should move to service/android.py and the port
  # should be finished.
  try:
    android_atom_to_build = commands.MarkAndroidAsStable(
        buildroot=buildroot,
        tracking_branch=tracking_branch,
        android_package=package_name,
        android_build_branch=android_build_branch,
        boards=boards,
        android_version=android_version,
        android_gts_build_branch=android_gts_build_branch)
  except commands.AndroidIsPinnedUprevError as e:
    # If the uprev failed due to a pin, CI needs to unpin and retry.
    android_atom_to_build = e.new_android_atom
    output_proto.status = android_pb2.MARK_STABLE_STATUS_PINNED

  if android_atom_to_build:
    CPV = portage_util.SplitCPV(android_atom_to_build)
    output_proto.android_atom.category = CPV.category
    output_proto.android_atom.package_name = CPV.package
    output_proto.android_atom.version = CPV.version
  else:
    output_proto.status = android_pb2.MARK_STABLE_STATUS_EARLY_EXIT


def UnpinVersion(_input_proto, _output_proto):
  """Unpin the Android version.

  See AndroidService documentation in api/proto/android.proto.

  Args:
    _input_proto (UnpinVersionRequest): The input proto. (not used.)
    _output_proto (google.protobuf.Empty): The output proto. (not used.)
  """

  osutils.SafeUnlink(ANDROIDPIN_MASK_PATH)
