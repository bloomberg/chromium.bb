# -*- coding: utf-8 -*-
# Copyright 2019 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Unittests for Binhost operations."""

from __future__ import print_function

import mock

from chromite.api.controller import binhost
from chromite.api.gen.chromite.api import binhost_pb2
from chromite.lib import cros_test_lib
from chromite.service import binhost as binhost_service

class PrepareBinhostUploadsTest(cros_test_lib.MockTestCase):
  """Unittests for PrepareBinhostUploads."""

  def setUp(self):
    self.PatchObject(binhost_service, 'GetPrebuiltsRoot',
                     return_value='/build/target/packages')
    self.PatchObject(binhost_service, 'GetPrebuiltsFiles',
                     return_value=['foo.tbz2', 'bar.tbz2'])
    self.PatchObject(binhost_service, 'UpdatePackageIndex',
                     return_value='/build/target/packages/Packages')

  def testPrepareBinhostUploads(self):
    """PrepareBinhostUploads returns Packages and tar files."""
    input_proto = binhost_pb2.PrepareBinhostUploadsRequest()
    input_proto.build_target.name = 'target'
    input_proto.uri = 'gs://chromeos-prebuilt/target'
    output_proto = binhost_pb2.PrepareBinhostUploadsResponse()
    binhost.PrepareBinhostUploads(input_proto, output_proto)
    self.assertEqual(output_proto.uploads_dir, '/build/target/packages')
    self.assertItemsEqual(
        [ut.path for ut in output_proto.upload_targets],
        ['Packages', 'foo.tbz2', 'bar.tbz2'])

  def testPrepareBinhostUploadsNonGsUri(self):
    """PrepareBinhostUploads dies when URI does not point to GS."""
    input_proto = binhost_pb2.PrepareBinhostUploadsRequest()
    input_proto.build_target.name = 'target'
    input_proto.uri = 'https://foo.bar'
    output_proto = binhost_pb2.PrepareBinhostUploadsResponse()
    with self.assertRaises(ValueError):
      binhost.PrepareBinhostUploads(input_proto, output_proto)


class SetBinhostTest(cros_test_lib.MockTestCase):
  """Unittests for SetBinhost."""

  def testSetBinhost(self):
    """SetBinhost calls service with correct args."""
    set_binhost = self.PatchObject(binhost_service, 'SetBinhost',
                                   return_value='/path/to/BINHOST.conf')

    input_proto = binhost_pb2.SetBinhostRequest()
    input_proto.build_target.name = 'target'
    input_proto.private = True
    input_proto.key = binhost_pb2.POSTSUBMIT_BINHOST
    input_proto.uri = 'gs://chromeos-prebuilt/target'

    output_proto = binhost_pb2.SetBinhostResponse()

    binhost.SetBinhost(input_proto, output_proto)

    self.assertEqual(output_proto.output_file, '/path/to/BINHOST.conf')
    self.assertEqual(
        set_binhost.call_args_list,
        [mock.call('target', 'POSTSUBMIT_BINHOST',
                   'gs://chromeos-prebuilt/target', private=True)])
