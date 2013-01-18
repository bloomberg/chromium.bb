#!/usr/bin/python
# Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os
import sys

CHROMITE_PATH = None

class ChromiteImporter(object):
  """Virtual chromite module

  If the checkout is not named 'chromite', trying to do 'from chromite.xxx'
  to import modules fails horribly.  Instead, manually locate the chromite
  directory (whatever it is named), load & return it whenever someone tries
  to import it.  This lets us use the stable name 'chromite' regardless of
  how things are structured on disk.

  This also lets us keep the sys.path search clean.  Otherwise we'd have to
  worry about what other dirs chromite were checked out near to as doing an
  import would also search those for .py modules.
  """

  # When trying to load the chromite dir from disk, we'll get called again,
  # so make sure to disable our logic to avoid an infinite loop.
  _loading = False

  def find_module(self, fullname, _path):
    """Handle the 'chromite' module"""
    if fullname == 'chromite' and not self._loading:
      return self
    return None

  def load_module(self, _fullname):
    """Return our cache of the 'chromite' module"""
    # Locate the top of the chromite dir by searching for the PRESUBMIT.cfg
    # file.  This assumes that file isn't found elsewhere in the tree.
    path = os.path.dirname(os.path.realpath(__file__))
    while not os.path.exists(os.path.join(path, 'PRESUBMIT.cfg')):
      path = os.path.dirname(path)

    # pylint: disable=W0603
    global CHROMITE_PATH
    CHROMITE_PATH = path + '/'

    # Finally load the chromite dir.
    path, mod = os.path.split(path)
    sys.path.insert(0, path)
    self._loading = True
    try:
      # This violates PEP302 slightly because __import__ will return the
      # cached module from sys.modules rather than reloading it from disk.
      # But the imp module does not work cleanly with meta_path currently
      # which makes it hard to use.  Until that is fixed, we won't bother
      # trying to address the edge case since it doesn't matter to us.
      return __import__(mod)
    finally:
      # We can't pop by index as the import might have changed sys.path.
      sys.path.remove(path)
      self._loading = False

sys.meta_path.insert(0, ChromiteImporter())

from chromite.lib import commandline


def FindTarget(target):
  # Turn the path into something we can import from the chromite tree.
  parent, target = os.path.split(target)
  parent = os.path.realpath(parent)
  assert parent.startswith(CHROMITE_PATH), (
      'could not figure out leading path\n'
      '\tparent: %s\n'
      '\tCHROMITE_PATH: %s' % (parent, CHROMITE_PATH))
  parent = parent[len(CHROMITE_PATH):].split(os.sep)
  target = ['chromite'] + parent + [target]
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
