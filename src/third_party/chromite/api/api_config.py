# -*- coding: utf-8 -*-
# Copyright 2019 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""API config object and related helper functionality."""

from __future__ import print_function

import sys

from chromite.api.gen.chromite.api import build_api_config_pb2


assert sys.version_info >= (3, 6), 'This module requires Python 3.6+'


class Error(Exception):
  """Base error class for the module."""


class UnknownCallTypeEnumValue(Error):
  """Thrown when the call type enum value in proto is not configured here."""


class ApiConfig(object):
  """API Config class."""
  # Call type constants.
  CALL_TYPE_EXECUTE = 1
  CALL_TYPE_VALIDATE_ONLY = 2
  CALL_TYPE_MOCK_SUCCESS = 3
  CALL_TYPE_MOCK_FAILURE = 4
  CALL_TYPE_MOCK_INVALID = 5

  # Maps the proto enum to the type constants.
  TYPE_ENUM_MAP = {
      build_api_config_pb2.CALL_TYPE_NONE: CALL_TYPE_EXECUTE,
      build_api_config_pb2.CALL_TYPE_EXECUTE: CALL_TYPE_EXECUTE,
      build_api_config_pb2.CALL_TYPE_VALIDATE_ONLY: CALL_TYPE_VALIDATE_ONLY,
      build_api_config_pb2.CALL_TYPE_MOCK_SUCCESS: CALL_TYPE_MOCK_SUCCESS,
      build_api_config_pb2.CALL_TYPE_MOCK_FAILURE: CALL_TYPE_MOCK_FAILURE,
      build_api_config_pb2.CALL_TYPE_MOCK_INVALID: CALL_TYPE_MOCK_INVALID,
  }

  # Maps the type constants to the proto enums.
  ENUM_TYPE_MAP = {
      CALL_TYPE_EXECUTE: build_api_config_pb2.CALL_TYPE_EXECUTE,
      CALL_TYPE_VALIDATE_ONLY: build_api_config_pb2.CALL_TYPE_VALIDATE_ONLY,
      CALL_TYPE_MOCK_SUCCESS: build_api_config_pb2.CALL_TYPE_MOCK_SUCCESS,
      CALL_TYPE_MOCK_FAILURE: build_api_config_pb2.CALL_TYPE_MOCK_FAILURE,
      CALL_TYPE_MOCK_INVALID: build_api_config_pb2.CALL_TYPE_MOCK_INVALID,
  }

  # The valid call types.
  _VALID_CALL_TYPES = tuple(ENUM_TYPE_MAP.keys())

  def __init__(self, call_type=CALL_TYPE_EXECUTE, log_path=None):
    assert call_type in self._VALID_CALL_TYPES
    self._call_type = call_type
    # Explicit `or None` to simplify proto default empty string.
    self.log_path = log_path or None

  def __eq__(self, other):
    if self.__class__ is other.__class__:
      return self.__dict__ == other.__dict__

    return NotImplemented

  __hash__ = NotImplemented

  @property
  def do_validation(self):
    # We skip validation for all mock calls, so do validation when it's
    # anything but a mocked call.
    return not (self.mock_call or self.mock_error or self.mock_invalid)

  @property
  def validate_only(self):
    return self._call_type == self.CALL_TYPE_VALIDATE_ONLY

  @property
  def mock_call(self):
    return self._call_type == self.CALL_TYPE_MOCK_SUCCESS

  @property
  def mock_error(self):
    return self._call_type == self.CALL_TYPE_MOCK_FAILURE

  @property
  def mock_invalid(self):
    return self._call_type == self.CALL_TYPE_MOCK_INVALID

  def get_proto(self, for_inside_execution=True):
    """Get the config as a proto.

    Args:
      for_inside_execution (bool): Allows avoiding propagating configs that are
        irrelevant for the build api process executed inside the chroot.
        Enabled by default.

    Returns:
      build_api_config_pb2.BuildApiConfig
    """
    config = build_api_config_pb2.BuildApiConfig()
    config.call_type = self.ENUM_TYPE_MAP[self._call_type]

    if not for_inside_execution:
      # Add values not needed when reexecuting.
      config.log_path = self.log_path

    return config


def build_config_from_proto(config_proto):
  """Build an ApiConfig instance from a BuildApiConfig message.

  Args:
    config_proto (build_api_config_pb2.BuildApiConfig): The proto config.

  Returns:
    ApiConfig
  """
  assert isinstance(config_proto, build_api_config_pb2.BuildApiConfig)

  if config_proto.call_type not in ApiConfig.TYPE_ENUM_MAP:
    raise UnknownCallTypeEnumValue('The given protobuf call_type value is not '
                                   'configured in api_config.')
  return ApiConfig(call_type=ApiConfig.TYPE_ENUM_MAP[config_proto.call_type],
                   log_path=config_proto.log_path)


class ApiConfigMixin(object):
  """Mixin to add an API Config factory properties.

  This is meant to be used for tests to make these configs more uniform across
  all the tests since there's very little to change anyway.
  """

  @property
  def api_config(self):
    return ApiConfig()

  @property
  def validate_only_config(self):
    return ApiConfig(call_type=ApiConfig.CALL_TYPE_VALIDATE_ONLY)

  @property
  def no_validate_config(self):
    return self.mock_call_config

  @property
  def mock_call_config(self):
    return ApiConfig(call_type=ApiConfig.CALL_TYPE_MOCK_SUCCESS)

  @property
  def mock_error_config(self):
    return ApiConfig(call_type=ApiConfig.CALL_TYPE_MOCK_FAILURE)
