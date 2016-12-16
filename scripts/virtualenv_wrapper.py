#!/usr/bin/env python2
# Copyright 2016 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Wrapper around chromite executable scripts that use virtualenv."""

from __future__ import print_function

import os
import subprocess
import sys

import wrapper


def _FindChromiteDir():
  path = os.path.dirname(os.path.realpath(__file__))
  while not os.path.exists(os.path.join(path, 'PRESUBMIT.cfg')):
    path = os.path.dirname(path)
  return path


_CHROMITE_DIR = _FindChromiteDir()

# _VIRTUALENV_DIR contains the scripts for working with venvs
_VIRTUALENV_DIR = os.path.join(_CHROMITE_DIR, '..', 'infra_virtualenv')
_CREATE_VENV_PATH = os.path.join(_VIRTUALENV_DIR, 'create_venv')

# _VENV_DIR is the virtualenv dir that contains bin/activate.
_VENV_DIR = os.path.join(_CHROMITE_DIR, 'venv', '.venv')
_VENV_PYTHON = os.path.join(_VENV_DIR, 'bin', 'python')
_REQUIREMENTS = os.path.join(_CHROMITE_DIR, 'venv', 'requirements.txt')

_VENV_MARKER = 'INSIDE_CHROMITE_VENV'


def main():
  if _IsInsideVenv(os.environ):
    wrapper.DoMain()
  else:
    _CreateVenv()
    _ExecInVenv(sys.argv)


def _CreateVenv():
  """Create or update chromite venv."""
  subprocess.check_call([
      _CREATE_VENV_PATH,
      _VENV_DIR,
      _REQUIREMENTS,
  ], stdout=sys.stderr)


def _ExecInVenv(args):
  """Exec command in chromite venv.

  Args:
    args: Sequence of arguments.
  """
  os.execve(
      _VENV_PYTHON,
      [_VENV_PYTHON] + list(args),
      _CreateVenvEnvironment(os.environ))


def _CreateVenvEnvironment(env_dict):
  """Create environment for a virtualenv.

  This adds a special marker variable to a copy of the input environment dict
  and returns the copy.

  Args:
    env_dict: Environment variable dict to use as base, which is not modified.

  Returns:
    New environment dict for a virtualenv.
  """
  new_env_dict = env_dict.copy()
  new_env_dict[_VENV_MARKER] = '1'
  return new_env_dict


def _IsInsideVenv(env_dict):
  """Return whether the environment dict is running inside a virtualenv.

  This checks the environment dict for the special marker added by
  _CreateVenvEnvironment().

  Args:
    env_dict: Environment variable dict to check

  Returns:
    A true value if inside virtualenv, else a false value.
  """
  # Checking sys.prefix or doing any kind of path check is unreliable because
  # we check out chromite to weird places.
  return env_dict.get(_VENV_MARKER, '')


if __name__ == '__main__':
  main()
