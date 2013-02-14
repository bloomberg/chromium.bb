#!/usr/bin/env python
# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""A helper script for setting up forwarding headers."""

import errno
import os
import sys


def GetHeaderFilesInDir(dir_path):
  """Return a list of all header files under dir_path
  (as absolute native paths)."""
  all_files = []
  for root, dirs, files in os.walk(dir_path):
    all_files.extend([os.path.join(root, f) for f in files if f.endswith('.h')])
  return all_files


def PathForInclude(path):
    # We should always use unix-style forward slashes in #includes.
    return path.replace(os.sep, '/')


def NativePath(path):
    return path.replace('/', os.sep)


def PathForGyp(path):
    # GYP will try to shell-escape backslashes, so we should always
    # return unix-style paths with forward slashes as the directory separators.
    return path.replace(os.sep, '/')


def Inputs(args):
  """List the files in the provided input dir.

  args: A list with 1 value, the input dir.
  Returns: 0 on success, other value on error."""
  if len(args) != 1:
    print "'inputs' expects only one input directory."
    return -1

  for filename in GetHeaderFilesInDir(args[0]):
    print PathForGyp(filename)
  return 0


def Outputs(args):
  """Takes an input dir and an output dir and figures out new output files
  based on copying from the input dir to the output dir.

  args: A list with 2 values, the input dir and the output dir.
  Returns: 0 on success, other value on error."""
  if len(args) != 2:
    print "'outputs' expects an input directory and an output directory."
    return -1

  base_input_dir = NativePath(args[0])
  output_dir = NativePath(args[1])
  input_files = GetHeaderFilesInDir(base_input_dir)
  for filename in input_files:
    rel_path = os.path.relpath(filename, base_input_dir)
    print PathForGyp(os.path.join(output_dir, rel_path))


def SetupHeaders(args):
  """Takes an input dir and an output dir and sets up forwarding headers
  from output dir to files in input dir.
  args: A list with 3 values, the input dir, the output dir, and the dir
      that #include paths will be relative to..
  Returns: 0 on success, other value on error."""
  if len(args) != 3:
    print ("'setup_headers' expects an input directory, an output directory, ."
        "and a directory to make includes relative to.")
    return -1

  base_input_dir = NativePath(args[0])
  output_dir = NativePath(args[1])
  relative_to_dir = NativePath(args[2])
  input_files = GetHeaderFilesInDir(base_input_dir)
  for input_filename in input_files:
    rel_path = os.path.relpath(input_filename, base_input_dir)
    out_filename = os.path.join(output_dir, rel_path)
    TryToMakeDir(os.path.split(out_filename)[0])
    WriteForwardingHeader(input_filename, out_filename, relative_to_dir)


def TryToMakeDir(dir_name):
  """Create the directory dir_name if it doesn't exist."""
  try:
    os.makedirs(dir_name)
  except OSError, e:
    if e.errno != errno.EEXIST:
      raise e


def WriteForwardingHeader(input_filename, out_filename, relative_to_dir):
  """Create a forwarding header from out_filename to input_filename."""
  # Windows has a file path limit of 260 characters, which can be hit when
  # generating these forwarding headers.  Instead of using an include
  # that specifies the path relative to out_filename's dir, we compute a path
  # relative to relative_to_dir, which must be included in gyp's include_dirs
  # settings for this to work. Even those this is really only needed on
  # Windows, we do this on all platforms to be consistent.
  rel_path = os.path.relpath(input_filename, relative_to_dir)
  out_file = open(out_filename, 'w')
  out_file.write("""// This file is generated.  Do not edit.
// The include is relative to "%s".
#include "%s"
""" % (os.path.abspath(relative_to_dir), PathForInclude(rel_path)))
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
