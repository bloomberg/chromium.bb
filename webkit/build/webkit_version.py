#!/usr/bin/python
# Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

'''Reads the Webkit Version.xcconfig file looking for MAJOR_VERSION and
MINOR_VERSION, emitting them into a webkit_version.h header file as
WEBKIT_MAJOR_VERSION and WEBKIT_MINOR_VERSION macros.
'''

import os
import re
import sys

sys.path.insert(0, '../../build/util')
import lastchange

def ReadVersionFile(fname):
  '''Reads the Webkit Version.xcconfig file looking for MAJOR_VERSION and
     MINOR_VERSION.  This function doesn't attempt to support the full syntax
     of xcconfig files.'''
  re_major = re.compile('MAJOR_VERSION\s*=\s*(\d+).*')
  re_minor = re.compile('MINOR_VERSION\s*=\s*(\d+).*')
  major = -1
  minor = -1
  f = open(fname, 'rb')
  line = "not empty"
  while line and not (major >= 0 and minor >= 0):
    line = f.readline()
    if major == -1:
      match = re_major.match(line)
      if match:
        major = int(match.group(1))
        continue
    if minor == -1:
      match = re_minor.match(line)
      if match:
        minor = int(match.group(1))
        continue
  assert(major >= 0 and minor >= 0)
  return (major, minor)

def GetWebKitRevision(webkit_dir, version_file):
  """Get the WebKit revision, in the form 'trunk@1234'."""

  # "svn info" tells us what we want, but third_party/WebKit does *not*
  # point at the upstream repo.  So instead we run svn info on the directory
  # containing the versioning file (which is some subdirectory of WebKit),
  # then strip that path back off of the resulting URL.
  version_file_dir = os.path.dirname(version_file)
  version_info = lastchange.FetchVersionInfo(
      default_lastchange=None,
      directory=os.path.join(webkit_dir, version_file_dir))

  # Now compute the real WebKit URL by stripping off the version file
  # directory from the URL we get out of version_info.
  # Further, we want to strip off the "http://svn..." from the left.
  # This is the root URL from the repository.
  assert version_info.url.startswith(version_info.root)
  assert version_info.url.endswith(version_file_dir)
  webkit_url = version_info.url[len(version_info.root):-len(version_file_dir)]
  webkit_url = webkit_url.strip('/')

  return "%s@%s" % (webkit_url, version_info.revision)


def EmitVersionHeader(webkit_dir, version_file, output_dir):
  '''Given webkit's version file, emit a header file that we can use from
  within webkit_glue.cc.
  '''

  # See .gypi file for discussion of this workaround for the version file.
  assert version_file[0] == '/'
  version_file = version_file[1:]

  major, minor = ReadVersionFile(os.path.join(webkit_dir, version_file))

  webkit_revision = GetWebKitRevision(webkit_dir, version_file)

  fname = os.path.join(output_dir, "webkit_version.h")
  f = open(fname, 'wb')
  template = """// webkit_version.h
// generated from %s

#define WEBKIT_VERSION_MAJOR %d
#define WEBKIT_VERSION_MINOR %d
#define WEBKIT_SVN_REVISION "%s"
""" % (version_file, major, minor, webkit_revision)
  f.write(template)
  f.close()

def main():
  EmitVersionHeader(*sys.argv[1:])


if __name__ == "__main__":
  main()


