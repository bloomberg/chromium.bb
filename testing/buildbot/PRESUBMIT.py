# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Enforces json format.

See http://dev.chromium.org/developers/how-tos/depottools/presubmit-scripts
for more details on the presubmit API built into depot_tools.
"""


def CommonChecks(input_api, output_api):
  commands = [
    input_api.Command(
      name='generate_buildbot_json', cmd=[
        input_api.python_executable, 'generate_buildbot_json.py', '--check'],
      kwargs={}, message=output_api.PresubmitError),

    input_api.Command(
      name='generate_buildbot_json_unittest', cmd=[
        input_api.python_executable, 'generate_buildbot_json_unittest.py'],
      kwargs={}, message=output_api.PresubmitError),

    input_api.Command(
      name='manage', cmd=[
        input_api.python_executable, 'manage.py', '--check'],
      kwargs={}, message=output_api.PresubmitError),
  ]
  messages = []

  # TODO(crbug.com/662541), TODO(crbug.com/792130): Make this command
  # run unconditionally once we've added coverage to .vpython and can
  # safely assume it will be importable.
  try:
    import coverage
    assert coverage  # This silences pylint.
    commands.append(input_api.Command(
        name='generate_buildbot_json_coveragetest',
        cmd=[input_api.python_executable,
             'generate_buildbot_json_coveragetest.py'],
        kwargs={}, message=output_api.PresubmitError))
  except ImportError:
    messages.append(output_api.PresubmitNotifyResult(
        'Python\'s coverage module is not installed; '
        'coverage tests in //testing/buildbot are disabled. '
        'To fix this on linux try running: '
        'sudo apt-get install python-coverage'))

  messages.extend(input_api.RunTests(commands))
  return messages


def CheckChangeOnUpload(input_api, output_api):
  return CommonChecks(input_api, output_api)


def CheckChangeOnCommit(input_api, output_api):
  return CommonChecks(input_api, output_api)
