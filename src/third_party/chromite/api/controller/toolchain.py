# -*- coding: utf-8 -*-
# Copyright 2019 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Toolchain-related operations."""

from __future__ import print_function

from chromite.api import validate
from chromite.api.gen.chromite.api import toolchain_pb2
from chromite.lib import toolchain_util

_VALID_ARTIFACT_TYPES = [toolchain_pb2.ORDERFILE, toolchain_pb2.KERNEL_AFDO]


@validate.require('build_target.name')
@validate.is_in('artifact_type', _VALID_ARTIFACT_TYPES)
@validate.validation_complete
def UpdateEbuildWithAFDOArtifacts(input_proto, output_proto, _config):
  """Update Chrome or kernel ebuild with most recent unvetted artifacts.

  Args:
    input_proto (VerifyAFDOArtifactsRequest): The input proto
    output_proto (VerifyAFDOArtifactsResponse): The output proto
    _config (api_config.ApiConfig): The API call config.
  """

  board = input_proto.build_target.name
  artifact_type = input_proto.artifact_type
  if artifact_type is toolchain_pb2.ORDERFILE:
    status = toolchain_util.OrderfileUpdateChromeEbuild(board)
  else:
    status = toolchain_util.AFDOUpdateKernelEbuild(board)
  output_proto.status = status


@validate.require('build_target.name')
@validate.is_in('artifact_type', _VALID_ARTIFACT_TYPES)
@validate.validation_complete
def UploadVettedAFDOArtifacts(input_proto, output_proto, _config):
  """Upload a vetted orderfile to GS bucket.

  Args:
    input_proto (VerifyAFDOArtifactsRequest): The input proto
    output_proto (VerifyAFDOArtifactsResponse): The output proto
    _config (api_config.ApiConfig): The API call config.
  """
  board = input_proto.build_target.name
  if input_proto.artifact_type is toolchain_pb2.ORDERFILE:
    artifact_type = 'orderfile'
  else:
    artifact_type = 'kernel_afdo'

  output_proto.status = toolchain_util.UploadAndPublishVettedAFDOArtifacts(
      artifact_type, board)
