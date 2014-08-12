# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Script that is used by PRESUBMIT.py to run style checks on Java files."""

import os
import subprocess


def RunCheckstyle(input_api, output_api, style_file):
  if not os.path.exists(style_file):
    file_error = ('  Java checkstyle configuration file is missing: '
                  + style_file)
    return [output_api.PresubmitError(file_error)]

  # Filter out non-Java files and files that were deleted.
  java_files = [x.LocalPath() for x in input_api.AffectedFiles(False, False)
                if os.path.splitext(x.LocalPath())[1] == '.java']
  if not java_files:
    return []

  # Run checkstyle
  checkstyle_env = os.environ.copy()
  checkstyle_env['JAVA_CMD'] = 'java'
  try:
    check = subprocess.Popen(['java', '-cp',
                              'third_party/checkstyle/checkstyle-5.7-all.jar',
                              'com.puppycrawl.tools.checkstyle.Main', '-c',
                              style_file] + java_files,
                             stdout=subprocess.PIPE, env=checkstyle_env)
    stdout, _ = check.communicate()
    if check.returncode == 0:
      return []
  except OSError as e:
    import errno
    if e.errno == errno.ENOENT:
      install_error = ('  checkstyle is not installed. Please run '
                       'build/install-build-deps-android.sh')
      return [output_api.PresubmitPromptWarning(install_error)]

  # Remove non-error values from stdout
  errors = stdout.splitlines()

  if errors and errors[0] == 'Starting audit...':
    del errors[0]
  if errors and errors[-1] == 'Audit done.':
    del errors[-1]

  # Filter out warnings
  errors = [x for x in errors if 'warning: ' not in x]
  if not errors:
    return []

  local_path = input_api.PresubmitLocalPath()
  output = []
  for error in errors:
    # Change the full file path to relative path in the output lines
    full_path, end = error.split(':', 1)
    rel_path = os.path.relpath(full_path, local_path)
    output.append('  %s:%s' % (rel_path, end))
  return [output_api.PresubmitPromptWarning('\n'.join(output))]
