#!/usr/bin/env python2
# -*- coding: utf-8 -*-
# Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Wrapper around chromite executable scripts.

This takes care of creating a consistent environment for chromite scripts
(like setting up import paths) so we don't have to duplicate the logic in
lots of places.
"""

from __future__ import print_function

import os
import sys


# Assert some minimum Python versions as we don't test or support any others.
# We only support Python 2.7, and require 2.7.5+/3.4+ to include signal fix:
# https://bugs.python.org/issue14173
if sys.version_info < (2, 7, 5):
  print('%s: chromite: error: Python-2.7.5+ is required' % (sys.argv[0],),
        file=sys.stderr)
  sys.exit(1)
elif sys.version_info.major == 3 and sys.version_info < (3, 4):
  # We don't actually test <Python-3.6.  Hope for the best!
  print('%s: chromite: error: Python-3.4+ is required' % (sys.argv[0],),
        file=sys.stderr)
  sys.exit(1)


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

  def find_module(self, fullname, _path=None):
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

    # pylint: disable=global-statement
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

# We have to put these imports after our meta-importer above.
# pylint: disable=wrong-import-position
from chromite.lib import commandline
from chromite.lib import cros_import


def FindTarget(target):
  """Turn the path into something we can import from the chromite tree.

  This supports a variety of ways of running chromite programs:
  # Loaded via depot_tools in $PATH.
  $ cros_sdk --help
  # Loaded via .../chromite/bin in $PATH.
  $ cros --help
  # No $PATH needed.
  $ ./bin/cros --help
  # Loaded via ~/bin in $PATH to chromite bin/ subdir.
  $ ln -s $PWD/bin/cros ~/bin; cros --help
  # No $PATH needed.
  $ ./cbuildbot/cbuildbot --help
  # No $PATH needed, but symlink inside of chromite dir.
  $ ln -s ./cbuildbot/cbuildbot; ./cbuildbot --help
  # Loaded via ~/bin in $PATH to non-chromite bin/ subdir.
  $ ln -s $PWD/cbuildbot/cbuildbot ~/bin/; cbuildbot --help
  # No $PATH needed, but a relative symlink to a symlink to the chromite dir.
  $ cd ~; ln -s bin/cbuildbot ./; ./cbuildbot --help
  # External chromite module
  $ ln -s ../chromite/scripts/wrapper.py foo; ./foo

  Args:
    target: Path to the script we're trying to run.

  Returns:
    The module main functor.
  """
  # We assume/require the script we're wrapping ends in a .py.
  full_path = target + '.py'
  while True:
    # Walk back one symlink at a time until we get into the chromite dir.
    parent, base = os.path.split(target)
    parent = os.path.realpath(parent)
    if parent.startswith(CHROMITE_PATH):
      target = base
      break
    target = os.path.join(os.path.dirname(target), os.readlink(target))

  # If we walked all the way back to wrapper.py, it means we're trying to run
  # an external module.  So we have to import it by filepath and not via the
  # chromite.xxx.yyy namespace.
  if target != 'wrapper.py':
    assert parent.startswith(CHROMITE_PATH), (
        'could not figure out leading path\n'
        '\tparent: %s\n'
        '\tCHROMITE_PATH: %s' % (parent, CHROMITE_PATH))
    parent = parent[len(CHROMITE_PATH):].split(os.sep)
    target = ['chromite'] + parent + [target]

    if target[1] == 'bin':
      # Convert chromite/bin/foo -> chromite/scripts/foo.
      # Since chromite/bin/ is in $PATH, we want to keep it clean.
      target[1] = 'scripts'
    elif target[1] == 'bootstrap' and len(target) == 3:
      # Convert <git_repo>/bootstrap/foo -> <git_repo>/bootstrap/scripts/foo.
      target.insert(2, 'scripts')

    try:
      module = cros_import.ImportModule(target)
    except ImportError as e:
      print(
          '%s: could not import chromite module: %s: %s' % (sys.argv[0],
                                                            full_path, e),
          file=sys.stderr)
      raise
  else:
    try:
      # Python 3 way.
      from importlib.machinery import SourceFileLoader
      _loader = lambda *args: SourceFileLoader(*args).load_module()
    except ImportError:
      # Python 2 way.
      import imp
      _loader = imp.load_source

    try:
      module = _loader('main', full_path)
    except IOError as e:
      print(
          '%s: could not import external module: %s: %s' % (sys.argv[0],
                                                            full_path, e),
          file=sys.stderr)
      raise

  # Run the module's main func if it has one.
  main = getattr(module, 'main', None)
  if main:
    return main

  # Is this a unittest?
  if target[-1].rsplit('_', 1)[-1] in ('test', 'unittest'):
    from chromite.lib import cros_test_lib
    return lambda _argv: cros_test_lib.main(module=module)


def DoMain():
  commandline.ScriptWrapperMain(FindTarget)


if __name__ == '__main__':
  DoMain()
