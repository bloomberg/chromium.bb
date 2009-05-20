#!/usr/bin/python
# Copyright (c) 2009 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Unit tests for depot_tools."""

mox = None

def OnTestsLoad():
  import os
  import sys
  old_path = sys.path
  global mox
  try:
    directory, _file = os.path.split(__file__)
    sys.path.append(os.path.abspath(os.path.join(directory, 'pymox')))
    sys.path.append(os.path.abspath(os.path.join(directory, '..')))
    try:
      import mox as Mox
      mox = Mox
    except ImportError:
      print "Trying to automatically checkout pymox."
      import subprocess
      subprocess.call(['svn', 'co', 'http://pymox.googlecode.com/svn/trunk',
                       os.path.join(directory, 'pymox')],
                      shell=True)
      try:
        import mox as Mox
        mox = Mox
      except ImportError:
        print >> sys.stderr, ("\nError, failed to load pymox\n")
        raise
  finally:
    # Restore the path
    sys.path = old_path

OnTestsLoad()
