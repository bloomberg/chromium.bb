# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os
import sys


ROOT_DIR = os.path.abspath(os.path.join(os.path.dirname(__file__), os.pardir))


def RegisterAllBackends():
  """Registers all the known backends."""
  from memory_inspector.core import backends
  from memory_inspector.backends.android import android_backend
  backends.Register(android_backend.AndroidBackend())


def _IncludeDeps():
  """Imports all the project dependencies."""
  chromium_dir = os.path.abspath(os.path.join(ROOT_DIR, os.pardir, os.pardir))
  assert(os.path.isdir(chromium_dir)), 'Cannot find chromium ' + chromium_dir

  sys.path += [
      ROOT_DIR,

      # Include all dependencies.
      os.path.join(chromium_dir, 'build', 'android'),  # For pylib.
  ]

_IncludeDeps()