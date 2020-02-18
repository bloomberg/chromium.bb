# -*- coding: utf-8 -*-
# Copyright 2019 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Control file generation for the autoupdate_EndToEnd server-side test.

This library is used by paygen_build_lib to generate the paygen_au_* control
files during the PaygenBuild* builder phase.
"""

from __future__ import print_function

import os
import re

from chromite.lib import constants


_name_re = re.compile(r'\s*NAME\s*=')
_autotest_test_name = 'autoupdate_EndToEndTest'


def generate_full_control_file(test, env, orig_control_code):
  """Returns the parameterized control file for the test config.

  Args:
    test: the test config object (TestConfig)
    env: the test environment parameters (TestEnv or None)
    orig_control_code: string containing the template control code

  Returns:
    Parameterized control file based on args (string)
  """
  orig_name = get_test_name()
  code_lines = orig_control_code.splitlines()
  for i, line in enumerate(code_lines):
    if _name_re.match(line):
      new_name = '%s_%s' % (orig_name, test.unique_name_suffix())
      code_lines[i] = line.replace(orig_name, new_name)
      break

  env_code_args = env.get_code_args() if env else ''
  return test.get_code_args() + env_code_args + '\n'.join(code_lines) + '\n'


def dump_autotest_control_file(test, env, control_code, directory):
  """Creates control file for test and returns the path to created file.

  Args:
    test: the test config object (TestConfig)
    env: the test environment parameters (TestEnv)
    control_code: string containing the template control code
    directory: the directory to dump the control file to

  Returns:
    Path to the newly dumped control file
  """
  if not os.path.exists(directory):
    os.makedirs(directory)

  parametrized_control_code = generate_full_control_file(
      test, env, control_code)

  control_file = os.path.join(directory,
                              test.get_control_file_name())
  with open(control_file, 'w') as fh:
    fh.write(parametrized_control_code)

  return control_file


def get_test_name():
  """Returns the name of the server side test."""
  return _autotest_test_name


def get_control_file_name():
  """Returns the path of the end-to-end base control file."""
  return os.path.join(
      constants.SOURCE_ROOT, 'src', 'third_party', 'autotest', 'files',
      'server', 'site_tests', get_test_name(), 'control')
