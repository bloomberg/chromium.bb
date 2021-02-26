# Copyright 2014 The LUCI Authors. All rights reserved.
# Use of this source code is governed under the Apache License, Version 2.0
# that can be found in the LICENSE file.

import logging
import os
import sys
import unittest

import six

# Directory client/tests/
TESTS_DIR = os.path.dirname(os.path.abspath(six.text_type(__file__)))

# Directory client/
CLIENT_DIR = os.path.dirname(TESTS_DIR)
sys.path.insert(0, CLIENT_DIR)

# Fix import path.
sys.path.insert(0, os.path.join(
    CLIENT_DIR, 'third_party', 'httplib2', 'python%d' % sys.version_info.major))
sys.path.insert(0, os.path.join(CLIENT_DIR, 'third_party', 'pyasn1'))
sys.path.insert(0, os.path.join(CLIENT_DIR, 'third_party', 'rsa'))
sys.path.insert(0, os.path.join(CLIENT_DIR, 'third_party'))

# third_party/
from depot_tools import fix_encoding

from utils import fs


_UMASK = None


class EnvVars(object):
  """Context manager for environment variables.

  Passed a dict to the constructor it sets variables named with the key to the
  value.  Exiting the context causes all the variables named with the key to be
  restored to their value before entering the context.
  """
  def __init__(self, var_map):
    self.var_map = var_map
    self._backup = None

  def __enter__(self):
    self._backup = os.environ
    os.environ = os.environ.copy()
    os.environ.update(self.var_map)

  def __exit__(self, exc_type, exc_value, traceback):
    os.environ = self._backup


class SymLink(str):
  """Used as a marker to create a symlink instead of a file."""


def umask():
  """Returns current process umask without modifying it."""
  global _UMASK
  if _UMASK is None:
    _UMASK = os.umask(0o777)
    os.umask(_UMASK)
  return _UMASK


def make_tree(out, contents):
  for relpath, content in sorted(contents.items()):
    filepath = os.path.join(out, relpath.replace('/', os.path.sep))
    dirpath = os.path.dirname(filepath)
    if not fs.isdir(dirpath):
      fs.makedirs(dirpath, 0o700)
    if isinstance(content, SymLink):
      fs.symlink(content, filepath)
    else:
      mode = 0o700 if relpath.endswith('.py') else 0o600
      flags = os.O_WRONLY | os.O_CREAT
      if sys.platform == 'win32':
        # pylint: disable=no-member
        flags |= os.O_BINARY
      with os.fdopen(os.open(filepath, flags, mode), 'wb') as f:
        f.write(six.ensure_binary(content))


def setup():
  fix_encoding.fix_encoding()
  # Use an unusual umask.
  os.umask(0o070)
  fs.chdir(TESTS_DIR)


def main():
  """Improvement over unittest.main()."""
  logging.basicConfig(
      level=logging.DEBUG if '-v' in sys.argv else logging.ERROR,
      format='%(levelname)5s %(filename)15s(%(lineno)3d): %(message)s')
  if '-v' in sys.argv:
    unittest.TestCase.maxDiff = None
  setup()
  unittest.main()
