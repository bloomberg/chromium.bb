# Copyright 2016 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Presubmit for android_webview/tools."""


def _GetPythonUnitTests(input_api, output_api):
  return input_api.canned_checks.GetUnitTestsRecursively(
      input_api, output_api,
      input_api.PresubmitLocalPath(),
      whitelist=['.*_test\\.py$'],
      blacklist=[])


def CommonChecks(input_api, output_api):
  """Presubmit checks run on both upload and commit.
  """
  checks = []
  checks.extend(input_api.canned_checks.GetPylint(
      input_api, output_api, pylintrc='pylintrc'))
  checks.extend(_GetPythonUnitTests(input_api, output_api))
  return input_api.RunTests(checks, False)


def CheckChangeOnUpload(input_api, output_api):
  """Presubmit checks on CL upload."""
  return CommonChecks(input_api, output_api)


def CheckChangeOnCommit(input_api, output_api):
  """Presubmit checks on commit."""
  return CommonChecks(input_api, output_api)
