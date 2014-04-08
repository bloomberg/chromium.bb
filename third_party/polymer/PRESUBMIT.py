# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Chromium presubmit script for third_party/polymer.

See http://dev.chromium.org/developers/how-tos/depottools/presubmit-scripts
for more details on the presubmit API built into gcl.
"""

def _CheckVulcanizeWasRun(input_api, output_api):
  """Checks to make sure vulcanize has been run on polymer when it is
     changed."""
  version_tag = '// @version: '
  separator = input_api.os_path.sep
  cwd = input_api.PresubmitLocalPath() + separator
  polymer_js = cwd + 'polymer' + separator + 'polymer.js'
  polymer_vulcanized_js = cwd + 'vulcanized' + separator + 'polymer-elements.js'

  version = ''
  with open(polymer_js, 'r') as f:
    for line in f:
      if version_tag in line:
        version = line.split(' ')[2].strip()
  if (not version):
    return [output_api.PresubmitError('Expected to find an @version tag in: ' +
                                      polymer_js + ' but did not.')]
  vulcanized_version = ''
  with open(polymer_vulcanized_js, 'r') as f:
    for line in f:
      if version_tag in line:
        vulcanized_version = line.split(' ')[2].strip()

  if (not vulcanized_version):
    return [output_api.PresubmitError('Expected to find an @version tag in: ' +
                                      polymer_vulcanized_js + ' but did not.')]
  if (vulcanized_version != version):
    return [output_api.PresubmitError('The vulcanized version of polymer (' +
        polymer_vulcanized_js + ') has version ' + vulcanized_version +
        ' while the original version (' + polymer_js + ') has version ' +
        version + '. You probably need to run vulcanize.py in ' + cwd + '.')]

  return []

def _CommonChecks(input_api, output_api):
  """Checks common to both upload and commit."""
  results = []
  results.extend(_CheckVulcanizeWasRun(input_api, output_api))
  return results

def CheckChangeOnUpload(input_api, output_api):
  results = []
  results.extend(_CommonChecks(input_api, output_api))
  return results


def CheckChangeOnCommit(input_api, output_api):
  results = []
  results.extend(_CommonChecks(input_api, output_api))
  return results
