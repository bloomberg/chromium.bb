# Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""
Tool module for adding, to a construction environment, Chromium-specific
wrappers around SCons builders.  This gives us a central place for any
customization we need to make to the different things we build.
"""

import sys

from SCons.Script import *

class Null(object):
  def __new__(cls, *args, **kwargs):
    if '_inst' not in vars(cls):
      cls._inst = super(type, cls).__new__(cls, *args, **kwargs)
    return cls._inst
  def __init__(self, *args, **kwargs): pass
  def __call__(self, *args, **kwargs): return self
  def __repr__(self): return "Null()"
  def __nonzero__(self): return False
  def __getattr__(self, name): return self
  def __setattr__(self, name, val): return self
  def __delattr__(self, name): return self
  def __getitem__(self, name): return self


def generate(env):
  # Add the grit tool to the base environment because we use this a lot.
  sys.path.append(env.Dir('$SRC_DIR/tools/grit').abspath)
  env.Tool('scons', toolpath=[env.Dir('$SRC_DIR/tools/grit/grit')])

  # Add the repack python script tool that we use in multiple places.
  sys.path.append(env.Dir('$SRC_DIR/tools/data_pack').abspath)
  env.Tool('scons', toolpath=[env.Dir('$SRC_DIR/tools/data_pack/')])

def exists(env):
  return True
