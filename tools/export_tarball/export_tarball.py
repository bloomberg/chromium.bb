#!/usr/bin/python
# Copyright (c) 2009 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""
This tool creates a tarball with all the sources, but without .svn directories.

It can also remove files which are not strictly required for build, so that
the resulting tarball can be reasonably small (last time it was ~110 MB).

Example usage:

export_tarball.py /foo/bar

The above will create file /foo/bar.tar.bz2.
"""

from __future__ import with_statement

import contextlib
import optparse
import os
import sys
import tarfile

NONESSENTIAL_DIRS = (
    'chrome/test/data',
    'chrome/tools/test/reference_build',
    'gears/binaries',
    'net/data/cache_tests',
    'o3d/documentation',
    'o3d/samples',
    'third_party/lighttpd',
    'third_party/WebKit/LayoutTests',
    'webkit/data/layout_tests',
    'webkit/tools/test/reference_build',
)

def GetSourceDirectory():
  return os.path.realpath(
    os.path.join(os.path.dirname(__file__), '..', '..', '..', 'src'))

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

  if not os.path.exists(GetSourceDirectory()):
    print 'Cannot find the src directory.'
    return 1

  output_fullname = args[0] + '.tar.bz2'
  output_basename = os.path.basename(args[0])

  def ShouldExcludePath(path):
    head, tail = os.path.split(path)
    if tail in ('.svn', '.git'):
      return True

    if not options.remove_nonessential_files:
      return False
    for nonessential_dir in NONESSENTIAL_DIRS:
      if path.startswith(os.path.join(GetSourceDirectory(), nonessential_dir)):
        return True

    return False

  with contextlib.closing(tarfile.open(output_fullname, 'w:bz2')) as archive:
    archive.add(GetSourceDirectory(), arcname=output_basename,
                exclude=ShouldExcludePath)

  return 0

if __name__ == "__main__":
  sys.exit(main(sys.argv[1:]))
