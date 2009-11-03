#!/usr/bin/env python
# Copyright (c) 2009 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Helper script to generate file lists for documentation.gyp."""

import os
import sys
import types


def AppendBasePath(folder, filenames):
  """Appends a base path to a ist of files"""
  return [os.path.join(folder, filename) for filename in filenames]


def GetIdlFiles():
  idl_list_filename = os.path.join('..', 'plugin', 'idl_list.manifest')
  idl_list_basepath = os.path.dirname(idl_list_filename)
  files = eval(open(idl_list_filename, "r").read())
  idl_files = AppendBasePath(idl_list_basepath, files)
  return idl_files


def GetJsFiles():
  js_list_filename = os.path.join('..', 'samples', 'o3djs', 'js_list.manifest')
  js_list_basepath = os.path.dirname(js_list_filename)
  files = eval(open(js_list_filename, "r").read())
  o3djs_files = AppendBasePath(js_list_basepath, files)
  return o3djs_files


# Read in the manifest files (which are just really simple python files),
# and scrape out the file lists.
# TODO(gspencer): Since we no longer use the scons build, we should
# rework this so that the lists are just python lists so we can just
# do a simple eval instead of having to emulate scons import.
def main(argv):
  files = []
  if argv[0] == '--js':
    files = GetJsFiles()
  if argv[0] == '--idl':
    files = GetIdlFiles()
  files.sort()
  for file in files:
    # gyp wants paths with slashes, not backslashes.
    print file.replace("\\", "/")


if __name__ == "__main__":
  main(sys.argv[1:])
