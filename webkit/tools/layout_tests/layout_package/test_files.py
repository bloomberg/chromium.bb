#!/usr/bin/env python
# Copyright (c) 2009 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""This module is used to find all of the layout test files used by Chromium
(across all platforms). It exposes one public function - GatherTestFiles() -
which takes an optional list of paths. If a list is passed in, the returned
list of test files is constrained to those found under the paths passed in,
i.e. calling GatherTestFiles(["LayoutTests/fast"]) will only return files
under that directory."""

import glob
import os
import path_utils

# When collecting test cases, we include any file with these extensions.
_supported_file_extensions = set(['.html', '.shtml', '.xml', '.xhtml', '.pl',
                                  '.php', '.svg'])
# When collecting test cases, skip these directories
_skipped_directories = set(['.svn', '_svn', 'resources', 'script-tests'])

# Top-level directories to shard when running tests.
ROOT_DIRECTORIES = set(['chrome', 'LayoutTests', 'pending'])

def GatherTestFiles(paths):
  """Generate a set of test files and return them.

  Args:
    paths: a list of command line paths relative to the webkit/tests
        directory. glob patterns are ok.
  """
  paths_to_walk = set()
  # if paths is empty, provide a pre-defined list.
  if not paths:
    paths = ROOT_DIRECTORIES
  for path in paths:
    # If there's an * in the name, assume it's a glob pattern.
    path = os.path.join(path_utils.LayoutTestsDir(path), path)
    if path.find('*') > -1:
      filenames = glob.glob(path)
      paths_to_walk.update(filenames)
    else:
      paths_to_walk.add(path)

  # Now walk all the paths passed in on the command line and get filenames
  test_files = set()
  for path in paths_to_walk:
    if os.path.isfile(path) and _HasSupportedExtension(path):
      test_files.add(os.path.normpath(path))
      continue

    for root, dirs, files in os.walk(path):
      # don't walk skipped directories and sub directories
      if os.path.basename(root) in _skipped_directories:
        del dirs[:]
        continue

      for filename in files:
        if _HasSupportedExtension(filename):
          filename = os.path.join(root, filename)
          filename = os.path.normpath(filename)
          test_files.add(filename)

  return test_files

def _HasSupportedExtension(filename):
  """Return true if filename is one of the file extensions we want to run a
  test on."""
  extension = os.path.splitext(filename)[1]
  return extension in _supported_file_extensions

