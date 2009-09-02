#!/usr/bin/python
# Copyright (c) 2009 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Presubmit script for perf_expectations.

See http://dev.chromium.org/developers/how-tos/depottools/presubmit-scripts for
details on the presubmit API built into gcl.
"""

UNIT_TESTS = [
  'tests.simplejson_unittest',
]

PERF_EXPECTATIONS = 'perf_expectations.json'

def CheckChangeOnUpload(input_api, output_api):
  run_tests = False
  for path in input_api.LocalPaths():
    if PERF_EXPECTATIONS == input_api.os_path.basename(path):
      run_tests = True

  output = []
  if run_tests:
    output.extend(input_api.canned_checks.RunPythonUnitTests(input_api,
                                                             output_api,
                                                             UNIT_TESTS))
  return output


def CheckChangeOnCommit(input_api, output_api):
  run_tests = False
  for path in input_api.LocalPaths():
    if PERF_EXPECTATIONS == input_api.os_path.basename(path):
      run_tests = True

  output = []
  if run_tests:
    output.extend(input_api.canned_checks.RunPythonUnitTests(input_api,
                                                             output_api,
                                                             UNIT_TESTS))
    output.extend(input_api.canned_checks.CheckDoNotSubmit(input_api,
                                                           output_api))
  return output
