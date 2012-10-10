#!/usr/bin/python
# Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os
import sys
# We want to use the correct version of libraries even when executed through a
# symlink.
path = os.path.realpath(__file__)
path = os.path.normpath(os.path.join(os.path.dirname(path), '..', '..'))
sys.path.insert(0, path)
del path

from chromite.lib import commandline

def _rindex(haystack, needle):
  if needle not in haystack:
    raise ValueError('%s not found' % needle)
  return len(haystack) - haystack[::-1].index(needle) - 1


def FindTarget(target):
  # Turn the path into something we can import from the chromite tree.
  target = target.split(os.sep)
  target = target[_rindex(target, 'chromite'):]
  # Our bin dir is just scripts stuff.
  if target[1] == 'bin':
    target[1] = 'scripts'

  module = __import__('.'.join(target))
  # __import__ gets us the root of the namespace import; walk our way up.
  for attr in target[1:]:
    module = getattr(module, attr)

  return getattr(module, 'main', None)


if __name__ == '__main__':
  commandline.ScriptWrapperMain(FindTarget)
