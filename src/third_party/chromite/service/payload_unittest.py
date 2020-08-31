# -*- coding: utf-8 -*-
# Copyright 2019 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Payload service tests."""

from __future__ import print_function

from chromite.api.gen.chromiumos import common_pb2
from chromite.api.gen.chromite.api import payload_pb2
from chromite.lib import cros_test_lib
from chromite.lib.paygen import paygen_payload_lib
from chromite.service import payload


class PayloadServiceTest(cros_test_lib.MockTestCase):
  """Unsigned payload generation tests."""

  def setUp(self):
    """Set up a payload test with the Run method mocked."""
    self.PatchObject(paygen_payload_lib.PaygenPayload, 'Run',
                     return_value=None)

    # Common build defs.
    self.src_build = payload_pb2.Build(version='1.0.0', bucket='test',
                                       channel='test-channel', build_target=
                                       common_pb2.BuildTarget(name='cave'))
    self.tgt_build = payload_pb2.Build(version='2.0.0', bucket='test',
                                       channel='test-channel', build_target=
                                       common_pb2.BuildTarget(name='cave'))

  def testUnsigned(self):
    """Test the happy path on unsigned images."""

    # Image defs.
    src_image = payload_pb2.UnsignedImage(
        build=self.src_build, image_type='BASE', milestone='R79')
    tgt_image = payload_pb2.UnsignedImage(
        build=self.tgt_build, image_type='BASE', milestone='R80')

    payload_config = payload.PayloadConfig(
        tgt_image=tgt_image,
        src_image=src_image,
        dest_bucket='test',
        verify=True,
        keyset=None,
        upload=True)

    payload_config.GeneratePayload()

  def testSigned(self):
    """Test the happy path on signed images."""

    # Image defs.
    src_image = payload_pb2.SignedImage(
        build=self.src_build, image_type='BASE', key='cave-mp-v4')
    tgt_image = payload_pb2.SignedImage(
        build=self.tgt_build, image_type='BASE', key='cave-mp-v4')

    payload_config = payload.PayloadConfig(
        tgt_image=tgt_image,
        src_image=src_image,
        dest_bucket='test',
        verify=True,
        keyset=None,
        upload=True)

    payload_config.GeneratePayload()

  def testFullUpdate(self):
    """Test the happy path on full updates."""

    # Image def.
    tgt_image = payload_pb2.UnsignedImage(
        build=self.tgt_build, image_type='BASE', milestone='R80')

    payload_config = payload.PayloadConfig(
        tgt_image=tgt_image,
        src_image=None,
        dest_bucket='test',
        verify=True,
        keyset=None,
        upload=True)

    payload_config.GeneratePayload()


class PayloadUtilitiesTest(cros_test_lib.TestCase):
  """Test utilities related to payloads."""

  def testImageType(self):
    """Test _ImageTypeToStr works."""
    # pylint: disable=protected-access
    self.assertEqual('base', payload._ImageTypeToStr(1))
    # pylint: enable=protected-access
