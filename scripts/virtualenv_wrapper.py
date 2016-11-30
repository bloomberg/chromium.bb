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
_VIRTUALENV_DIR = os.path.join(_CHROMITE_DIR, '../infra_virtualenv')
# _VENV_DIR is the actual virtualenv that contains bin/activate.
_VENV_DIR = os.path.join(_CHROMITE_DIR, '.venv')
_REQUIREMENTS = os.path.join(_CHROMITE_DIR, 'venv', 'requirements.txt')


def main():
  if _IsInsideVenv():
    wrapper.DoMain()
  else:
    _CreateVenv()
    _ExecInVenv(sys.argv)


def _CreateVenv():
  """Create or update chromite venv."""
  subprocess.check_call([
      os.path.join(_VIRTUALENV_DIR, 'create_venv'),
      _VENV_DIR,
      _REQUIREMENTS,
  ], stdout=sys.stderr)


def _ExecInVenv(args):
  """Exec command in chromite venv.

  Args:
    args: Sequence of arguments.
  """
  os.execv(os.path.join(_VIRTUALENV_DIR, 'venv_command'),
           ['venv_command', _VENV_DIR] + list(args))


def _IsInsideVenv():
  """Return whether we're running inside a virtualenv."""
  # Proper way is checking sys.prefix and sys.base_prefix in Python 3.
  # PEP 405 isn't fully implemented in Python 2.
  return _VENV_DIR in os.environ.get('VIRTUAL_ENV', '')


if __name__ == '__main__':
  main()
