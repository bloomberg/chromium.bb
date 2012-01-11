#!/usr/bin/env python
# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""A helper script for setting up forwarding headers."""

import errno
import os
import sys


def GetHeaderFilesInDir(dir_path):
  """Return a list of all header files in dir_path."""
  all_files = []
  for root, dirs, files in os.walk(dir_path):
    # Backslashes get shell escaped by gyp, so force forward slash for
    # path separators.
    all_files.extend([os.path.join(root, f).replace(os.sep, '/')
                      for f in files if f.endswith('.h')])
  return all_files


def Inputs(args):
  """List the files in the provided input dir.

  args: A list with 1 value, the input dir.
  Returns: 0 on success, other value on error."""
  if len(args) != 1:
    print "'inputs' expects only one input directory."
    return -1

  for filename in GetHeaderFilesInDir(args[0]):
    print filename
  return 0


def Outputs(args):
  """Takes an input dir and an output dir and figures out new output files
  based on copying from the input dir to the output dir.

  args: A list with 2 values, the input dir and the output dir.
  Returns: 0 on success, other value on error."""
  if len(args) != 2:
    print "'outputs' expects an input directory and an output directory."
    return -1

  base_input_dir = args[0]
  output_dir = args[1]
  input_files = GetHeaderFilesInDir(base_input_dir)
  for filename in input_files:
    rel_path = filename[len(base_input_dir) + 1:]
    # Backslashes get shell escaped by gyp, so force forward slash for
    # path separators.
    print os.path.join(output_dir, rel_path).replace(os.sep, '/')


def SetupHeaders(args):
  """Takes an input dir and an output dir and sets up forwarding headers
  from output dir to files in input dir.
  args: A list with 2 values, the input dir and the output dir.
  Returns: 0 on success, other value on error."""
  if len(args) != 2 and len(args) != 3:
    print ("'setup_headers' expects an input directory, an output directory, ."
        "and an optional directory to make includes relative to.")
    return -1

  base_input_dir = args[0]
  output_dir = args[1]
  relative_to_dir = None
  if len(args) == 3:
    relative_to_dir = args[2]
  input_files = GetHeaderFilesInDir(base_input_dir)
  for input_filename in input_files:
    rel_path = input_filename[len(base_input_dir) + 1:]
    out_filename = os.path.join(output_dir, rel_path)
    TryToMakeDir(os.path.split(out_filename)[0])
    WriteSetupFilename(input_filename, out_filename, relative_to_dir)


def TryToMakeDir(dir_name):
  """Create the directory dir_name if it doesn't exist."""
  try:
    os.makedirs(dir_name)
  except OSError, e:
    if e.errno != errno.EEXIST:
      raise e


def NormalizePath(path):
  """Normalize path for use with os.path.commonprefix.
  On windows, this makes sure that the drive letters are always in the
  same case."""
  abs_path = os.path.abspath(path)
  drive, rest = os.path.splitdrive(abs_path)
  return os.path.join(drive.lower(), rest)


def WriteSetupFilename(input_filename, out_filename, relative_to_dir):
  """Create a forwarding header from out_filename to input_filename."""
  # Figure out the relative path from out_filename to input_filename.
  # We can't use os.path.relpath since that's a python2.6 feature and we
  # support python 2.5.
  input_filename = NormalizePath(input_filename)
  out_filename = NormalizePath(out_filename)
  ancestor = os.path.commonprefix([input_filename, out_filename])

  assert os.path.isdir(ancestor)
  num_parent_dirs = 0
  out_dir = os.path.split(out_filename)[0]
  while os.path.normpath(ancestor) != os.path.normpath(out_dir):
    num_parent_dirs += 1
    out_dir = os.path.split(out_dir)[0]

  if sys.platform == 'win32':
    # Windows has a file path limit of 260 characters that we can hit when
    # generating these forwarding headers.  Instead of writing the full
    # relative path, just write the path relative to the WebKit/chromium dir,
    # which is in the include path.
    rel_path = input_filename[len(ancestor):]
    if relative_to_dir:
      rel_path = os.path.join(relative_to_dir, rel_path)
  else:
    rel_path = os.path.join('/'.join(['..'] * num_parent_dirs),
                            input_filename[len(ancestor):])

  out_file = open(out_filename, 'w')
  out_file.write("""// This file is generated.  Do not edit.
#include "%s"
""" % rel_path)
  out_file.close()


def Main(argv):
  commands = {
    'inputs': Inputs,
    'outputs': Outputs,
    'setup_headers': SetupHeaders,
  }
  command = argv[1]
  args = argv[2:]
  return commands[command](args)


if __name__ == '__main__':
  sys.exit(Main(sys.argv))
