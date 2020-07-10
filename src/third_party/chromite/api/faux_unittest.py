# -*- coding: utf-8 -*-
# Copyright 2019 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Tests for the faux module."""

from __future__ import print_function

from chromite.api import faux
from chromite.api.api_config import ApiConfigMixin
from chromite.api.gen.chromite.api import build_api_test_pb2
from chromite.lib import cros_test_lib


class MockResponsesTest(cros_test_lib.TestCase, ApiConfigMixin):
  """Tests for faux's mock response functionality."""

  _IMPL_RESULT = 'result'
  _SUCCESS_RESULT = 'success'
  _ERROR_RESULT = 'error'
  _ALL_RESULT = 'all'

  def setUp(self):
    self.request = build_api_test_pb2.TestRequestMessage()
    self.response = build_api_test_pb2.TestResultMessage()

  def _faux_success(self, _input_proto, output_proto, _config):
    """Faux success method."""
    output_proto.result = self._SUCCESS_RESULT

  def _faux_error(self, _input_proto, output_proto, _config):
    """Faux error method."""
    output_proto.result = self._ERROR_RESULT

  def _faux_all(self, _input_proto, output_proto, config):
    """All responses method."""
    self.assertIn(config, [self.mock_call_config, self.mock_error_config])
    output_proto.result = self._ALL_RESULT

  def test_call_called(self):
    """Test a faux call."""

    @faux.error(self._faux_error)
    @faux.success(self._faux_success)
    def impl(_input_proto, _output_proto, _config):
      self.fail('Implementation was called.')

    impl(self.request, self.response, self.mock_call_config)

    self.assertEqual(self.response.result, self._SUCCESS_RESULT)

  def test_error_called(self):
    """Test the faux error intercepts the call."""
    @faux.success(self._faux_success)
    @faux.error(self._faux_error)
    def impl(_input_proto, _output_proto, _config):
      self.fail('Implementation was called.')

    impl(self.request, self.response, self.mock_error_config)

    self.assertEqual(self.response.result, self._ERROR_RESULT)

  def test_impl_called(self):
    """Test the call is not mocked when not requested."""
    @faux.error(self._faux_error)
    @faux.success(self._faux_success)
    def impl(_input_proto, output_proto, _config):
      output_proto.result = self._IMPL_RESULT

    impl(self.request, self.response, self.api_config)

    self.assertEqual(self.response.result, self._IMPL_RESULT)

  def test_all_responses_success(self):
    """Test the call is intercepted by the all responses decorator."""

    @faux.all_responses(self._faux_all)
    def impl(_input_proto, _output_proto, _config):
      self.fail('Implementation was called.')

    impl(self.request, self.response, self.mock_call_config)
    self.assertEqual(self.response.result, self._ALL_RESULT)

  def test_all_responses_error(self):
    """Test the call is intercepted by the all responses decorator."""

    @faux.all_responses(self._faux_all)
    def impl(_input_proto, _output_proto, _config):
      self.fail('Implementation was called.')

    impl(self.request, self.response, self.mock_error_config)
    self.assertEqual(self.response.result, self._ALL_RESULT)

  def test_all_responses_impl(self):
    """Test the call is intercepted by the all responses decorator."""

    @faux.all_responses(self._faux_all)
    def impl(_input_proto, output_proto, _config):
      output_proto.result = self._IMPL_RESULT

    impl(self.request, self.response, self.api_config)
    self.assertEqual(self.response.result, self._IMPL_RESULT)
