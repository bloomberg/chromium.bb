# Copyright 2019 The PDFium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Presubmit script for PDFium testing tools.

See http://dev.chromium.org/developers/how-tos/depottools/presubmit-scripts
for more details on the presubmit API built into depot_tools.
"""


def _CommonChecks(input_api, output_api):
  tests = []
  tests.extend(input_api.canned_checks.GetPylint(input_api, output_api))
  return tests


def CheckChangeOnUpload(input_api, output_api):
  tests = []
  tests.extend(_CommonChecks(input_api, output_api))
  return input_api.RunTests(tests)


def CheckChangeOnCommit(input_api, output_api):
  tests = []
  tests.extend(_CommonChecks(input_api, output_api))
  return input_api.RunTests(tests)
