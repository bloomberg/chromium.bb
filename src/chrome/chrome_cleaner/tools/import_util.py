# Copyright 2018 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Utilities for importing python proto modules."""

import os
import sys


_CURRENT_DIRECTORY = os.path.dirname(os.path.abspath(__file__))
_ROOT_DIRECTORY = os.path.join(_CURRENT_DIRECTORY, os.pardir, os.pardir)


def _InSourceTree():
  return os.path.basename(os.path.abspath(_ROOT_DIRECTORY)) == 'src'


def AddImportPath(path):
  """Adds the given path to the front of the Python import path."""
  sys.path.insert(0, os.path.normpath(path))


def GetBuildDirectory(build_configuration):
  """Returns the path to the build directory relative to this file.

  Args:
    build_configuration: name of the build configuration whose directory
        should be looked up, e.g. 'Debug' or 'Release'.

  Returns:
    Path to the build directory or None if used from outside the source tree.
  """
  if not _InSourceTree():
    return None
  return os.path.join(_ROOT_DIRECTORY, 'out', build_configuration)


def AddProtosToPath(root_path):
  """Adds generated protobuf modules to the Python import path.

  Args:
    root_path: root directory where the pyproto subdir is located.
  """
  assert root_path is not None
  # Add the root pyproto dir so Python packages under that dir (such as
  # google.protobuf, which is built by //third_party/protobuf:py_proto) can be
  # imported with fully-qualified package names. (eg. "from google.protobuf
  # import text_format".)
  pyproto_dir = os.path.join(root_path, 'pyproto')
  AddImportPath(pyproto_dir)

  # Add individual directories with proto files that can be imported with bare
  # names. (eg. "import file_digest_pb2" to import file_digest.proto from the
  # chrome/chrome_cleaner/proto dir.)
  AddImportPath(os.path.join(pyproto_dir, 'chrome', 'chrome_cleaner', 'proto'))
  AddImportPath(os.path.join(pyproto_dir, 'chrome', 'chrome_cleaner', 'logging',
                             'proto'))


def AddDepotToolsToPath():
  """Locates a depot_tools checkout and adds it to the Python import path.

  Returns:
    The path to depot_tools.
  """

  # Try to import the local copy of find_depot_tools from the import subdir, in
  # case this is being run from a checkout that hasn't been synced.
  AddImportPath(os.path.join(_CURRENT_DIRECTORY, 'import'))

  # But prefer to use the copy synced into the build dir which might be more
  # up-to-date. (This path will take priority over the path added above.)
  if _InSourceTree():
    AddImportPath(os.path.join(_ROOT_DIRECTORY, 'build'))

  # Import the first copy of find_depot_tools found in the above paths.
  import find_depot_tools
  return find_depot_tools.add_depot_tools_to_path()
