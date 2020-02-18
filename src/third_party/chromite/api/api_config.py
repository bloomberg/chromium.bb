# -*- coding: utf-8 -*-
# Copyright 2019 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""API config object and related helper functionality."""

from __future__ import print_function


class ApiConfig(object):
  """API Config class."""

  def __init__(self, validate_only=False, mock_call=False, mock_error=False):
    assert [validate_only, mock_call, mock_error].count(True) <= 1
    self.validate_only = validate_only
    self.mock_call = mock_call
    self.mock_error = mock_error
    self._is_mock = self.mock_call or self.mock_error

  def __eq__(self, other):
    if self.__class__ is other.__class__:
      return ((self.validate_only, self.mock_call, self.mock_error) ==
              (other.validate_only, other.mock_call, other.mock_error))

    return NotImplemented

  __hash__ = NotImplemented

  @property
  def do_validation(self):
    return not self._is_mock


class ApiConfigMixin(object):
  """Mixin to add an API Config factory properties.

  This is meant to be used for tests to make these configs more uniform across
  all of the tests since there's very little to change anyway.
  """

  @property
  def api_config(self):
    return ApiConfig()

  @property
  def validate_only_config(self):
    return ApiConfig(validate_only=True)

  @property
  def no_validate_config(self):
    return self.mock_call_config

  @property
  def mock_call_config(self):
    return ApiConfig(mock_call=True)

  @property
  def mock_error_config(self):
    return ApiConfig(mock_error=True)
