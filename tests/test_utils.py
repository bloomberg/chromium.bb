# Copyright 2014 The Swarming Authors. All rights reserved.
# Use of this source code is governed under the Apache License, Version 2.0 that
# can be found in the LICENSE file.

import os

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


def umask():
  """Returns current process umask without modifying it."""
  global _UMASK
  if _UMASK is None:
    _UMASK = os.umask(0777)
    os.umask(_UMASK)
  return _UMASK
