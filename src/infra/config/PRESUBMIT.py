# Copyright (c) 2018 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.


def _CheckForTranslations(input_api, output_api):
  del output_api

  return []

def _CommonChecks(input_api, output_api):
  commands = []
  # TODO(martiniss): Move this check to the root presubmit. It checks to ensure
  # that path regexps in cq.cfg reference something locally.
  for f in input_api.AffectedFiles():
    local_path = f.LocalPath()
    if local_path.endswith('cq.cfg'):
      commands.append(
        input_api.Command(
          name='cq.cfg presubmit', cmd=[
              input_api.python_executable, 'branch/cq_cfg_presubmit.py',
              '--check'],
          kwargs={}, message=output_api.PresubmitError),
      )

  results = []

  results.extend(input_api.canned_checks.CheckChangedLUCIConfigs(
      input_api, output_api))
  results.extend(input_api.RunTests(commands))
  return results

def CheckChangeOnUpload(input_api, output_api):
  return _CommonChecks(input_api, output_api)

def CheckChangeOnCommit(input_api, output_api):
  return _CommonChecks(input_api, output_api)
