# -*- coding: utf-8 -*-
# Copyright 2018 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Chromite Buildbucket V2 client wrapper and helpers.

The client constructor and methods here are tailored to Chromite's
usecases. Other users should, instead, prefer to copy the Python
client out of lib/luci/prpc and third_party/infra_libs/buildbucket.
"""

from __future__ import print_function

from google.protobuf import field_mask_pb2

from chromite.lib import cros_logging as logging
from chromite.lib.luci.prpc.client import Client

from infra_libs.buildbucket.proto import rpc_pb2
from infra_libs.buildbucket.proto.rpc_prpc_pb2 import BuildsServiceDescription

BBV2_URL_ENDPOINT_PROD = (
    "cr-buildbucket.appspot.com"
)
BBV2_URL_ENDPOINT_TEST = (
    "cr-buildbucket-test.appspot.com"
)

def UpdateSelfBuildPropertiesNonBlocking(key, value):
  """Updates the build.output.properties with key:value through a service.

  Butler is a ChOps service that reads in logs and updates Buildbucket of the
  properties. This method has no guarantees on the timeliness of updating
  the property.

  Args:
    key: name of the property.
    value: value of the property.
  """
  logging.PrintKitchenSetBuildProperty(key, value)

def UpdateSelfCommonBuildProperties(
    critical=None, chrome_version=None, milestone_version=None,
    platform_version=None, full_version=None, toolchain_url=None,
    build_type=None, unibuild=None, suite_scheduling=None):
  """Update build.output.properties for the current build.

  Sends the property values to buildbucket via
  UpdateSelfBuildPropertiesNonBlocking.

  Args:
    critical: (Optional) |important| flag of the build.
    chrome_version: (Optional) version of chrome of the build. Eg "74.0.3687.0".
    milestone_version: (Optional) milestone version of  of the build. Eg "74".
    platform_version: (Optional) platform version of the build. Eg "11671.0.0".
    full_version: (Optional) full version of the build.
        Eg "R74-11671.0.0-b3416654".
    toolchain_url: (Optional) toolchain_url of the build.
    build_type: (Optional) One of ('paladin', 'full', 'canary', 'pre_cq',...).
    unibuild: (Optional) Boolean indicating whether build is unibuild.
    suite_scheduling: (Optional)
  """
  if critical is not None:
    UpdateSelfBuildPropertiesNonBlocking('critical', critical)
  if chrome_version is not None:
    UpdateSelfBuildPropertiesNonBlocking('chrome_version', chrome_version)
  if milestone_version is not None:
    UpdateSelfBuildPropertiesNonBlocking('milestone_version', milestone_version)
  if platform_version is not None:
    UpdateSelfBuildPropertiesNonBlocking('platform_version', platform_version)
  if full_version is not None:
    UpdateSelfBuildPropertiesNonBlocking('full_version', full_version)
  if toolchain_url is not None:
    UpdateSelfBuildPropertiesNonBlocking('toolchain_url', toolchain_url)
  if build_type is not None:
    UpdateSelfBuildPropertiesNonBlocking('build_type', build_type)
  if unibuild is not None:
    UpdateSelfBuildPropertiesNonBlocking('unibuild', unibuild)
  if suite_scheduling is not None:
    UpdateSelfBuildPropertiesNonBlocking('suite_scheduling', suite_scheduling)

def UpdateBuildMetadata(metadata):
  """Update build.output.properties from a CBuildbotMetadata instance.

  The function further uses UpdateSelfCommonBuildProperties and has hence
  no guarantee of timely updation.

  Args:
    metadata: CBuildbot Metadata instance to update with.
  """
  d = metadata.GetDict()
  versions = d.get('version') or {}
  UpdateSelfCommonBuildProperties(
      chrome_version=versions.get('chrome'),
      milestone_version=versions.get('milestone'),
      platform_version=versions.get('platform'),
      full_version=versions.get('full'),
      toolchain_url=d.get('toolchain-url'),
      build_type=d.get('build_type'),
      critical=d.get('important'),
      unibuild=d.get('unibuild', False),
      suite_scheduling=d.get('suite_scheduling', False))

class BuildbucketV2(object):
  """Connection to Buildbucket V2 database."""

  def __init__(self, test_env=False):
    """Constructor for Buildbucket V2 Build client.

    Args:
      test_env: Whether to have the client connect to test URL endpoint on GAE.
    """
    if test_env:
      self.client = Client(BBV2_URL_ENDPOINT_TEST, BuildsServiceDescription)
    else:
      self.client = Client(BBV2_URL_ENDPOINT_PROD, BuildsServiceDescription)

  def GetBuildStatus(self, buildbucket_id, properties=None):
    """GetBuildStatus of a specific build with buildbucket_id.

    Args:
      buildbucket_id: id of the build in buildbucket.
      properties: specific build.output.properties to query.

    Returns:
      The corresponding Build proto. See here:
      https://chromium.googlesource.com/infra/luci/luci-go/+/master/buildbucket/proto/build.proto
    """
    if properties:
      field_mask = field_mask_pb2.FieldMask(paths=[properties])
      get_build_request = rpc_pb2.GetBuildRequest(id=buildbucket_id,
                                                  fields=field_mask)
    else:
      get_build_request = rpc_pb2.GetBuildRequest(id=buildbucket_id)
    return self.client.GetBuild(get_build_request)
