#!/usr/bin/env python
# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Cleans up a swarm slave after the tests have run."""

import errno
import glob
import os
import tempfile
import time
import shutil
import subprocess
import sys


# This is copied from Chromium's project build/scripts/common/chromium_utils.py.
def RemoveDirectory(*path):
  """Recursively removes a directory, even if it's marked read-only.

  Remove the directory located at *path, if it exists.

  shutil.rmtree() doesn't work on Windows if any of the files or directories
  are read-only, which svn repositories and some .svn files are.  We need to
  be able to force the files to be writable (i.e., deletable) as we traverse
  the tree.

  Even with all this, Windows still sometimes fails to delete a file, citing
  a permission error (maybe something to do with antivirus scans or disk
  indexing).  The best suggestion any of the user forums had was to wait a
  bit and try again, so we do that too.  It's hand-waving, but sometimes it
  works. :/
  """
  file_path = os.path.join(*path)
  if not os.path.exists(file_path):
    return

  if sys.platform == 'win32':
    # Give up and use cmd.exe's rd command.
    file_path = os.path.normcase(file_path)
    for _ in xrange(3):
      if not subprocess.call(['cmd.exe', '/c', 'rd', '/q', '/s', file_path]):
        break
      time.sleep(3)
    return

  def RemoveWithRetry_non_win(rmfunc, path):
    if os.path.islink(path):
      return os.remove(path)
    else:
      return rmfunc(path)

  remove_with_retry = RemoveWithRetry_non_win

  def RmTreeOnError(function, path, excinfo):
    """This works around a problem whereby python 2.x on Windows has no ability
    to check for symbolic links. os.path.islink() always returns False but
    shutil.rmtree() will fail if invoked on a symbolic link whose target was
    deleted before the link. E.g., reproduce like this:
    > mkdir test
    > mkdir test\1
    > mklink /D test\current test\1
    > python -c "import shutl; shutil.rmtree('test')"
    To avoid this issue, we pass this error-handling function to rmtree. If we
    see the exact sort of failure, we ignore it.  All other failures we
    re-raise.
    """
    exception_type = excinfo[0]
    exception_value = excinfo[1]
    # If shutil.rmtree encounters a symbolic link on Windows, os.listdir will
    # fail with a WindowsError exception with an ENOENT errno (i.e., file not
    # found).  We'll ignore that error.  Note that WindowsError is not defined
    # for non-Windows platforms, so we use OSError (of which it is a subclass)
    # to avoid lint complaints about an undefined global on non-Windows
    # platforms.
    if (function is os.listdir) and issubclass(exception_type, OSError):
      if exception_value.errno == errno.ENOENT:
        # File does not exist, and we're trying to delete, so we can ignore the
        # failure.
        print 'WARNING:  Failed to list %s during rmtree.  Ignoring.\n' % path
      else:
        raise
    else:
      raise

  for root, dirs, files in os.walk(file_path, topdown=False):
    # For POSIX:  making the directory writable guarantees removability.
    # Windows will ignore the non-read-only bits in the chmod value.
    os.chmod(root, 0770)
    for name in files:
      remove_with_retry(os.remove, os.path.join(root, name))
    for name in dirs:
      remove_with_retry(lambda p: shutil.rmtree(p, onerror=RmTreeOnError),
                        os.path.join(root, name))

  remove_with_retry(os.rmdir, file_path)


def delete(filename):
  try:
    if os.path.isdir(filename):
      RemoveDirectory(filename)
    else:
      os.remove(filename)
  except OSError:
    pass


def main():
  for filename in glob.iglob('*.zip'):
    delete(filename)

  # Clear the temp directory.
  for filename in glob.iglob(os.path.join(tempfile.gettempdir(), '*')):
    delete(filename)

  # Clears up stale run_isolated.py temporary directories.
  for filename in glob.iglob('run_tha_test*'):
    delete(filename)
  print ''
  return 0


if __name__ == '__main__':
  sys.exit(main())
