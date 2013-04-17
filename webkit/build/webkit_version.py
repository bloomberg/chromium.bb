#!/usr/bin/env python
# Copyright (c) 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

'''Emits a webkit_version.h header file with
WEBKIT_MAJOR_VERSION and WEBKIT_MINOR_VERSION macros.
'''

import os
import re
import sys

# Get the full path of the current script which would be something like
# src/webkit/build/webkit_version.py and navigate backwards twice to strip the
# last two path components to get to the srcroot.
# This is to ensure that the script can load the lastchange module by updating
# the sys.path variable with the desired location.
path = os.path.dirname(os.path.realpath(__file__))
path = os.path.dirname(os.path.dirname(path))
path = os.path.join(path, 'build', 'util')

sys.path.insert(0, path)
import lastchange


def GetWebKitRevision(webkit_src_dir):
  """Get the WebKit revision, in the form 'trunk@1234'."""

  version_info = lastchange.FetchVersionInfo(
      default_lastchange=None,
      directory=webkit_src_dir,
      directory_regex_prior_to_src_url='webkit')

  if version_info.url == None:
    version_info.url = 'Unknown URL'
  version_info.url = version_info.url.strip('/')

  if version_info.revision == None:
    version_info.revision = '0'

  return "%s@%s" % (version_info.url, version_info.revision)


def EmitVersionHeader(webkit_src_dir, output_dir):
  '''Emit a header file that we can use from within webkit_glue.cc.'''

  # These are hard-coded from when we forked Blink. Presumably these
  # would be better in a header somewhere.
  major, minor = (537, 36)
  webkit_revision = GetWebKitRevision(webkit_src_dir)

  fname = os.path.join(output_dir, "webkit_version.h")
  f = open(fname, 'wb')
  template = """// webkit_version.h
// generated from %s

#define WEBKIT_VERSION_MAJOR %d
#define WEBKIT_VERSION_MINOR %d
#define WEBKIT_SVN_REVISION "%s"
""" % (webkit_src_dir, major, minor, webkit_revision)
  f.write(template)
  f.close()
  return 0


def main():
  return EmitVersionHeader(*sys.argv[1:])


if __name__ == "__main__":
  sys.exit(main())
