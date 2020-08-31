# -*- coding: utf-8 -*-
# Copyright 2020 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Tests for message_util."""

from __future__ import print_function

import os
import sys

from chromite.api import message_util
from chromite.api.gen.chromite.api import build_api_test_pb2
from chromite.lib import cros_test_lib


assert sys.version_info >= (3, 6), 'This module requires Python 3.6+'


class JsonSerializerTest(cros_test_lib.TestCase):
  """Tests for the JSON serializer."""

  def test_serialization(self):
    """Test json format serialization/deserialization."""
    serializer = message_util.JsonSerializer()

    # Build a message.
    msg = build_api_test_pb2.TestRequestMessage()
    msg.id = 'foo'

    # Round trip the message contents through the serializer.
    serialized = serializer.serialize(msg)
    deserialized = build_api_test_pb2.TestRequestMessage()
    serializer.deserialize(serialized, deserialized)

    # Create an identical message manually.
    source = '{"id":"foo"}'
    deserialized2 = build_api_test_pb2.TestRequestMessage()
    serializer.deserialize(source, deserialized2)

    # Make sure the round tripped data is equal, and matches the manually
    # constructed version.
    self.assertEqual(deserialized, msg)
    self.assertEqual(deserialized, deserialized2)


class BinarySerializerTest(cros_test_lib.TestCase):
  """Tests for the binary serializer."""

  def test_serialization(self):
    """Test binary format serialization/deserialization."""
    serializer = message_util.BinarySerializer()

    # Build a message.
    msg = build_api_test_pb2.TestRequestMessage()
    msg.id = 'foo'

    # Round trip the message contents through the serializer.
    serialized = serializer.serialize(msg)
    deserialized = build_api_test_pb2.TestRequestMessage()
    serializer.deserialize(serialized, deserialized)

    # Make sure the round tripped data is unchanged.
    self.assertEqual(msg, deserialized)


class MessageHandlerTest(cros_test_lib.TempDirTestCase):
  """MessageHandler tests."""

  def test_binary_serialization(self):
    """Test binary serialization/deserialization."""
    msg_path = os.path.join(self.tempdir, 'proto')

    # Use the message handler configured in the module to avoid config drift.
    handler = message_util.get_message_handler(msg_path,
                                               message_util.FORMAT_BINARY)

    msg = build_api_test_pb2.TestRequestMessage()
    msg.id = 'foo'

    # Round trip the data.
    self.assertNotExists(msg_path)
    handler.write_from(msg)
    self.assertExists(msg_path)

    deserialized = build_api_test_pb2.TestRequestMessage()
    handler.read_into(deserialized)

    # Make sure the data has not mutated.
    self.assertEqual(msg, deserialized)

  def test_json_serialization(self):
    """Test json serialization/deserialization."""
    msg_path = os.path.join(self.tempdir, 'proto')

    # Use the message handler configured in the module to avoid config drift.
    handler = message_util.get_message_handler(msg_path,
                                               message_util.FORMAT_JSON)

    msg = build_api_test_pb2.TestRequestMessage()
    msg.id = 'foo'

    # Round trip the data.
    self.assertNotExists(msg_path)
    handler.write_from(msg)
    self.assertExists(msg_path)

    deserialized = build_api_test_pb2.TestRequestMessage()
    handler.read_into(deserialized)

    # Make sure the data has not mutated.
    self.assertEqual(msg, deserialized)
