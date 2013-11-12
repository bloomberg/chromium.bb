#!/usr/bin/env python
# Copyright 2013 The Swarming Authors. All rights reserved.
# Use of this source code is governed under the Apache License, Version 2.0 that
# can be found in the LICENSE file.

import logging
import os
import shutil
import sys
import tempfile
import unittest

ROOT_DIR = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, ROOT_DIR)

import run_isolated


def write_content(filepath, content):
  with open(filepath, 'wb') as f:
    f.write(content)


class RunIsolatedTest(unittest.TestCase):
  def setUp(self):
    super(RunIsolatedTest, self).setUp()
    self.tempdir = tempfile.mkdtemp(prefix='run_isolated_test')
    logging.debug(self.tempdir)

  def tearDown(self):
    for dirpath, dirnames, filenames in os.walk(self.tempdir, topdown=True):
      for filename in filenames:
        run_isolated.set_read_only(os.path.join(dirpath, filename), False)
      for dirname in dirnames:
        run_isolated.set_read_only(os.path.join(dirpath, dirname), False)
    shutil.rmtree(self.tempdir)
    super(RunIsolatedTest, self).tearDown()

  def assertFileMode(self, filepath, mlinux, mosx, mwin):
    # Note that it depends on umask.
    actual = os.stat(filepath).st_mode
    if sys.platform == 'win32':
      expected = mwin
    elif sys.platform == 'darwin':
      expected = mosx
    else:
      expected = mlinux
    self.assertEqual(expected, actual, (filepath, oct(expected), oct(actual)))

  def test_umask(self):
    # Check assumptions about umask. Anyone can override umask so this test is
    # bound to be brittle. In practice if it fails, it means assertFileMode()
    # will have to be updated accordingly.
    umask = os.umask(0)
    os.umask(umask)
    if sys.platform == 'darwin':
      self.assertEqual(oct(022), oct(umask))
    elif sys.platform == 'win32':
      self.assertEqual(oct(0), oct(umask))
    else:
      self.assertEqual(oct(02), oct(umask))

  def test_delete_wd_rf(self):
    # Confirms that a RO file in a RW directory can be deleted on non-Windows.
    dir_foo = os.path.join(self.tempdir, 'foo')
    file_bar = os.path.join(dir_foo, 'bar')
    os.mkdir(dir_foo, 0777)
    write_content(file_bar, 'bar')
    run_isolated.set_read_only(dir_foo, False)
    run_isolated.set_read_only(file_bar, True)
    self.assertFileMode(dir_foo, 040775, 040755, 040777)
    self.assertFileMode(file_bar, 0100400, 0100400, 0100444)
    if sys.platform == 'win32':
      # On Windows, a read-only file can't be deleted.
      with self.assertRaises(OSError):
        os.remove(file_bar)
    else:
      os.remove(file_bar)

  def test_delete_rd_wf(self):
    # Confirms that a Rw file in a RO directory can be deleted on Windows only.
    dir_foo = os.path.join(self.tempdir, 'foo')
    file_bar = os.path.join(dir_foo, 'bar')
    os.mkdir(dir_foo, 0777)
    write_content(file_bar, 'bar')
    run_isolated.set_read_only(dir_foo, True)
    run_isolated.set_read_only(file_bar, False)
    self.assertFileMode(dir_foo, 040500, 040500, 040555)
    self.assertFileMode(file_bar, 0100664, 0100644, 0100666)
    if sys.platform == 'win32':
      # A read-only directory has a convoluted meaning on Windows, it means that
      # the directory is "personalized". This is used as a signal by Windows
      # Explorer to tell it to look into the directory for desktop.ini.
      # See http://support.microsoft.com/kb/326549 for more details.
      # As such, it is important to not try to set the read-only bit on
      # directories on Windows since it has no effect other than trigger
      # Windows Explorer to look for desktop.ini, which is unnecessary.
      os.remove(file_bar)
    else:
      with self.assertRaises(OSError):
        os.remove(file_bar)

  def test_delete_rd_rf(self):
    # Confirms that a RO file in a RO directory can't be deleted.
    dir_foo = os.path.join(self.tempdir, 'foo')
    file_bar = os.path.join(dir_foo, 'bar')
    os.mkdir(dir_foo, 0777)
    write_content(file_bar, 'bar')
    run_isolated.set_read_only(dir_foo, True)
    run_isolated.set_read_only(file_bar, True)
    self.assertFileMode(dir_foo, 040500, 040500, 040555)
    self.assertFileMode(file_bar, 0100400, 0100400, 0100444)
    with self.assertRaises(OSError):
      # It fails for different reason depending on the OS. See the test cases
      # above.
      os.remove(file_bar)

  def test_hard_link_mode(self):
    # Creates a hard link, see if the file mode changed on the node or the
    # directory entry.
    dir_foo = os.path.join(self.tempdir, 'foo')
    file_bar = os.path.join(dir_foo, 'bar')
    file_link = os.path.join(dir_foo, 'link')
    os.mkdir(dir_foo, 0777)
    write_content(file_bar, 'bar')
    run_isolated.hardlink(file_bar, file_link)
    self.assertFileMode(file_bar, 0100664, 0100644, 0100666)
    self.assertFileMode(file_link, 0100664, 0100644, 0100666)
    run_isolated.set_read_only(file_bar, True)
    self.assertFileMode(file_bar, 0100400, 0100400, 0100444)
    self.assertFileMode(file_link, 0100400, 0100400, 0100444)
    # This is bad news for Windows; on Windows, the file must be writeable to be
    # deleted, but the file node is modified. This means that every hard links
    # must be reset to be read-only after deleting one of the hard link
    # directory entry.


if __name__ == '__main__':
  logging.basicConfig(
      level=logging.DEBUG if '-v' in sys.argv else logging.ERROR)
  unittest.main()
