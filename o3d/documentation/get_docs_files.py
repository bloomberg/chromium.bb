#!/usr/bin/env python
# Copyright (c) 2009 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Helper script to generate file lists for documentation.gyp."""

import os
import sys
import types


GlobalsDict = { }


def UpdateGlobals(dict):
  """Copies pairs from dict into GlobalDict."""
  for i, v in dict.items():
    GlobalsDict.__setitem__(i, v)


def AppendBasePath(folder, filenames):
  """Appends a base path to a ist of files"""
  return [os.path.join(folder, filename) for filename in filenames]


def GetCallingNamespaces():
  """Return the locals and globals for the function that called
  into this module in the current call stack."""
  try: 1/0
  except ZeroDivisionError:
    # Don't start iterating with the current stack-frame to
    # prevent creating reference cycles (f_back is safe).
    frame = sys.exc_info()[2].tb_frame.f_back

  # Find the first frame that *isn't* from this file
  while frame.f_globals.get("__name__") == __name__:
    frame = frame.f_back

  return frame.f_locals, frame.f_globals


def ComputeExports(exports):
  """Compute a dictionary of exports given one of the parameters
  to the Export() function or the exports argument to SConscript()."""

  loc, glob = GetCallingNamespaces()

  retval = {}
  try:
    for export in exports:
      if isinstance(export, types.DictType):
        retval.update(export)
      else:
        try:
          retval[export] = loc[export]
        except KeyError:
          retval[export] = glob[export]
  except KeyError, x:
    raise Error, "Export of non-existent variable '%s'"%x

  return retval


def Export(*vars):
  """Copies the named variables to GlobalDict."""
  for var in vars:
    UpdateGlobals(ComputeExports(vars))


def Import(filename):
  """Imports a python file in a scope with 'Export' defined."""
  scope = {'__builtins__': globals()['__builtins__'],
           'Export': Export}
  file = open(filename, 'r')
  exec file in scope
  file.close()


def GetIdlFiles():
  idl_list_filename = os.path.join('..', 'plugin', 'idl_list.manifest')
  idl_list_basepath = os.path.dirname(idl_list_filename)
  Import(idl_list_filename)
  idl_files = AppendBasePath(idl_list_basepath, GlobalsDict['O3D_IDL_SOURCES'])
  return idl_files


def GetJsFiles():
  js_list_filename = os.path.join('..', 'samples', 'o3djs', 'js_list.manifest')
  js_list_basepath = os.path.dirname(js_list_filename)
  Import(js_list_filename)
  o3djs_files = AppendBasePath(js_list_basepath, GlobalsDict['O3D_JS_SOURCES'])
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
