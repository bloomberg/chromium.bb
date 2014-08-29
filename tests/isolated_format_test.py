#!/usr/bin/env python
# Copyright 2014 The Swarming Authors. All rights reserved.
# Use of this source code is governed under the Apache License, Version 2.0 that
# can be found in the LICENSE file.

import hashlib
import logging
import os
import shutil
import sys
import tempfile
import unittest

# net_utils adjusts sys.path.
import net_utils

import isolated_format


ALGO = hashlib.sha1


class TestCase(net_utils.TestCase):
  def test_get_hash_algo(self):
    # Tests here assume ALGO is used for default namespaces, check this
    # assumption.
    self.assertIs(isolated_format.get_hash_algo('default'), ALGO)
    self.assertIs(isolated_format.get_hash_algo('default-gzip'), ALGO)


class SymlinkTest(unittest.TestCase):
  def setUp(self):
    super(SymlinkTest, self).setUp()
    self.old_cwd = os.getcwd()
    self.cwd = tempfile.mkdtemp(prefix='isolate_')
    # Everything should work even from another directory.
    os.chdir(self.cwd)

  def tearDown(self):
    try:
      os.chdir(self.old_cwd)
      shutil.rmtree(self.cwd)
    finally:
      super(SymlinkTest, self).tearDown()

  if sys.platform == 'darwin':
    def test_expand_symlinks_path_case(self):
      # Ensures that the resulting path case is fixed on case insensitive file
      # system.
      os.symlink('dest', os.path.join(self.cwd, 'link'))
      os.mkdir(os.path.join(self.cwd, 'Dest'))
      open(os.path.join(self.cwd, 'Dest', 'file.txt'), 'w').close()

      result = isolated_format.expand_symlinks(unicode(self.cwd), 'link')
      self.assertEqual((u'Dest', [u'link']), result)
      result = isolated_format.expand_symlinks(
          unicode(self.cwd), 'link/File.txt')
      self.assertEqual((u'Dest/file.txt', [u'link']), result)

    def test_expand_directories_and_symlinks_path_case(self):
      # Ensures that the resulting path case is fixed on case insensitive file
      # system. A superset of test_expand_symlinks_path_case.
      # Create *all* the paths with the wrong path case.
      basedir = os.path.join(self.cwd, 'baseDir')
      os.mkdir(basedir.lower())
      subdir = os.path.join(basedir, 'subDir')
      os.mkdir(subdir.lower())
      open(os.path.join(subdir, 'Foo.txt'), 'w').close()
      os.symlink('subDir', os.path.join(basedir, 'linkdir'))
      actual = isolated_format.expand_directories_and_symlinks(
          unicode(self.cwd), [u'baseDir/'], lambda _: None, True, False)
      expected = [
        u'basedir/linkdir',
        u'basedir/subdir/Foo.txt',
        u'basedir/subdir/Foo.txt',
      ]
      self.assertEqual(expected, actual)

    def test_file_to_metadata_path_case_simple(self):
      # Ensure the symlink dest is saved in the right path case.
      subdir = os.path.join(self.cwd, 'subdir')
      os.mkdir(subdir)
      linkdir = os.path.join(self.cwd, 'linkdir')
      os.symlink('subDir', linkdir)
      actual = isolated_format.file_to_metadata(
          unicode(linkdir.upper()), {}, True, ALGO)
      expected = {'l': u'subdir', 'm': 360, 't': int(os.stat(linkdir).st_mtime)}
      self.assertEqual(expected, actual)

    def test_file_to_metadata_path_case_complex(self):
      # Ensure the symlink dest is saved in the right path case. This includes 2
      # layers of symlinks.
      basedir = os.path.join(self.cwd, 'basebir')
      os.mkdir(basedir)

      linkeddir2 = os.path.join(self.cwd, 'linkeddir2')
      os.mkdir(linkeddir2)

      linkeddir1 = os.path.join(basedir, 'linkeddir1')
      os.symlink('../linkedDir2', linkeddir1)

      subsymlinkdir = os.path.join(basedir, 'symlinkdir')
      os.symlink('linkedDir1', subsymlinkdir)

      actual = isolated_format.file_to_metadata(
          unicode(subsymlinkdir.upper()), {}, True, ALGO)
      expected = {
        'l': u'linkeddir1', 'm': 360, 't': int(os.stat(subsymlinkdir).st_mtime),
      }
      self.assertEqual(expected, actual)

      actual = isolated_format.file_to_metadata(
          unicode(linkeddir1.upper()), {}, True, ALGO)
      expected = {
        'l': u'../linkeddir2', 'm': 360, 't': int(os.stat(linkeddir1).st_mtime),
      }
      self.assertEqual(expected, actual)

  if sys.platform != 'win32':
    def test_symlink_input_absolute_path(self):
      # A symlink is outside of the checkout, it should be treated as a normal
      # directory.
      # .../src
      # .../src/out -> .../tmp/foo
      # .../tmp
      # .../tmp/foo
      src = os.path.join(self.cwd, u'src')
      src_out = os.path.join(src, 'out')
      tmp = os.path.join(self.cwd, 'tmp')
      tmp_foo = os.path.join(tmp, 'foo')
      os.mkdir(src)
      os.mkdir(tmp)
      os.mkdir(tmp_foo)
      # The problem was that it's an absolute path, so it must be considered a
      # normal directory.
      os.symlink(tmp, src_out)
      open(os.path.join(tmp_foo, 'bar.txt'), 'w').close()
      actual = isolated_format.expand_symlinks(src, u'out/foo/bar.txt')
      self.assertEqual((u'out/foo/bar.txt', []), actual)


if __name__ == '__main__':
  if '-v' in sys.argv:
    unittest.TestCase.maxDiff = None
  logging.basicConfig(
      level=(logging.DEBUG if '-v' in sys.argv else logging.ERROR))
  unittest.main()
