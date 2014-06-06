# Copyright 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Presubmit script for changes affecting tools/perf/.

See http://dev.chromium.org/developers/how-tos/depottools/presubmit-scripts
for more details about the presubmit API built into gcl (and git-cl).
"""

import os
import sys

PYLINT_BLACKLIST = []
PYLINT_DISABLED_WARNINGS = [
    'R0923',  # Interface not implemented
    'R0201',  # Method could be a function
    'E1101',  # Non-existent member is accessed.
]


def _CommonChecks(input_api, output_api):
  """Performs common checks, which includes running pylint."""
  results = []
  old_sys_path = sys.path
  try:
    # Modules in tools/perf depend on telemetry.
    sys.path = [os.path.join(os.pardir, 'telemetry')] + sys.path
    results.extend(input_api.canned_checks.RunPylint(
        input_api, output_api,
        black_list=PYLINT_BLACKLIST,
        disabled_warnings=PYLINT_DISABLED_WARNINGS))
    results.extend(_CheckJson(input_api, output_api))
  finally:
    sys.path = old_sys_path
  return results


def _CheckJson(input_api, output_api):
  """Checks whether JSON files in this change can be parsed."""
  for affected_file in input_api.AffectedFiles(include_deletes=False):
    filename = affected_file.AbsoluteLocalPath()
    if os.path.splitext(filename)[1] != '.json':
      continue
    try:
      input_api.json.load(open(filename))
    except ValueError:
      return [output_api.PresubmitError('Error parsing JSON in %s!' % filename)]
  return []


def CheckChangeOnUpload(input_api, output_api):
  report = []
  report.extend(_CommonChecks(input_api, output_api))
  return report


def CheckChangeOnCommit(input_api, output_api):
  report = []
  report.extend(_CommonChecks(input_api, output_api))
  return report
