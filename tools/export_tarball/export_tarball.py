#!/usr/bin/python
# Copyright (c) 2009 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""
This tool creates a tarball with all the sources, but without .svn directories.

It can also remove files which are not strictly required for build, so that
the resulting tarball can be reasonably small (last time it was ~110 MB).

Example usage (make sure gclient is in your PATH):

export_tarball.py /foo/bar

The above will create file /foo/bar.tar.bz2.
"""

from __future__ import with_statement

import contextlib
import optparse
import os
import shutil
import subprocess
import sys
import tarfile
import tempfile

def RunCommand(argv):
  """Runs the command with given argv and returns exit code."""
  try:
    proc = subprocess.Popen(argv, stdout=None)
  except OSError:
    return 1
  output = proc.communicate()[0]
  return proc.returncode

def main(argv):
  parser = optparse.OptionParser()
  parser.add_option("--remove-nonessential-files",
                    dest="remove_nonessential_files",
                    action="store_true", default=False)

  options, args = parser.parse_args(argv)

  if len(args) != 1:
    print 'You must provide only one argument: output file name'
    print '(without .tar.bz2 extension).'
    return 1

  output_fullname = args[0] + '.tar.bz2'
  output_basename = os.path.basename(args[0])
  target_dir = tempfile.mkdtemp()

  try:
    if RunCommand(['gclient', 'export', target_dir]) != 0:
      print 'gclient failed'
      return 1

    if options.remove_nonessential_files:
      nonessential_dirs = (
          'src/chrome/test/data',
          'src/chrome/tools/test/reference_build',
          'src/gears/binaries',
          'src/net/data/cache_tests',
          'src/o3d/documentation',
          'src/o3d/samples',
          'src/third_party/lighttpd',
          'src/third_party/WebKit/LayoutTests',
          'src/webkit/data/layout_tests',
          'src/webkit/tools/test/reference_build',
      )
      for dir in nonessential_dirs:
        path = os.path.join(target_dir, dir)
        try:
          print 'removing %s...' % dir
          shutil.rmtree(path)
        except OSError, e:
          print 'error while trying to remove %s, skipping' % dir

    with contextlib.closing(tarfile.open(output_fullname, 'w:bz2')) as archive:
      archive.add(os.path.join(target_dir, 'src'), arcname=output_basename)
  finally:
    shutil.rmtree(target_dir)

  return 0

if __name__ == "__main__":
  sys.exit(main(sys.argv[1:]))
