#!/usr/bin/python
# Copyright (c) 2012 The Native Client Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Setup for PyAuto NaCl tests.
This file was copied from src/chrome/test/functional/pyauto_functional.py

Use the following in your scripts to run them standalone:

# This should be at the top
import pyauto_nacl

if __name__ == '__main__':
  pyauto_nacl.Main()

This script can be used as an executable to fire off other scripts, similar
to unittest.py
  python pyauto_nacl.py test_script
"""

import os
import sys


def _LocatePyAutoDir():

  def _UpdatePythonPath(path):
    sys.path.append(path)

    # This ensures that any python scripts spawned by pyauto use the same
    # PYTHONPATH.
    if 'PYTHONPATH' in os.environ:
      os.environ['PYTHONPATH'] += os.pathsep + path
    else:
      os.environ['PYTHONPATH'] = path

  src_root = os.path.join(os.path.dirname(__file__),
                          os.pardir, os.pardir, os.pardir)
  _UpdatePythonPath(os.path.join(src_root, 'chrome/test/pyautolib'))
  _UpdatePythonPath(os.path.join(src_root, 'third_party'))

  # argv[1] is assumed to be the path to the pyautolib files downloaded from
  # the chrome continuous builder. It is removed because pyauto consumes the
  # rest of the command line and does not expect this argument.
  _UpdatePythonPath(sys.argv.pop(1))

_LocatePyAutoDir()

try:
  import pyauto
except ImportError:
  print >>sys.stderr, 'Cannot import pyauto from %s' % sys.path
  raise


class Main(pyauto.Main):
  """Main program for running PyAuto functional tests."""

  def __init__(self):
    # Make scripts in this dir importable
    sys.path.append(os.path.dirname(__file__))
    pyauto.Main.__init__(self)

  def TestsDir(self):
    return os.path.dirname(__file__)


if __name__ == '__main__':
  Main()
