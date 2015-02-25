# Copyright 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Presubmit script for changes affecting tools/perf/.

See http://dev.chromium.org/developers/how-tos/depottools/presubmit-scripts
for more details about the presubmit API built into depot_tools.
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
    results.extend(_CheckWprShaFiles(input_api, output_api))
  finally:
    sys.path = old_sys_path
  return results


def _CheckWprShaFiles(input_api, output_api):
  """Check whether the wpr sha files have matching URLs."""
  from telemetry.util import cloud_storage
  results = []
  for affected_file in input_api.AffectedFiles(include_deletes=False):
    filename = affected_file.AbsoluteLocalPath()
    if not filename.endswith('wpr.sha1'):
      continue
    expected_hash = cloud_storage.ReadHash(filename)
    is_wpr_file_uploaded = any(
        cloud_storage.Exists(bucket, expected_hash)
        for bucket in cloud_storage.BUCKET_ALIASES.itervalues())
    if not is_wpr_file_uploaded:
      wpr_filename = filename[:-5]
      results.append(output_api.PresubmitError(
          'There is no URLs matched for wpr sha file %s.\n'
          'You can upload your new wpr archive file with the command:\n'
          'depot_tools/upload_to_google_storage.py --bucket '
          '<Your pageset\'s bucket> %s.\nFor more info: see '
          'http://www.chromium.org/developers/telemetry/'
          'record_a_page_set#TOC-Upload-the-recording-to-Cloud-Storage' %
          (filename, wpr_filename)))
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
