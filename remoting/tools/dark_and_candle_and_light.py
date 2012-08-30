#!/usr/bin/env python
# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Run 'dark', 'candle', and 'light', to confirm that an msi can be unpacked
repacked successfully."""

from optparse import OptionParser
import os
import subprocess
import sys

def run(command):
  popen = subprocess.Popen(
      command, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
  out, _ = popen.communicate()
  return popen.returncode, out

def main():
  parser = OptionParser()
  parser.add_option('--wix_path', dest='wix_path')
  parser.add_option('--input', dest='input')
  parser.add_option('--intermediate_dir', dest='intermediate_dir')
  parser.add_option('--output', dest='output')
  options, args = parser.parse_args()
  if args:
    parser.error("no positional arguments expected")
  parameters = dict(options.__dict__)

  parameters['basename'] = os.path.splitext(os.path.basename(options.output))[0]

  dark_template = ('"%(wix_path)s\\dark" ' +
                   '-nologo ' +
                   '"%(input)s" ' +
                   '-o "%(intermediate_dir)s/%(basename)s.wxs" ' +
                   '-x "%(intermediate_dir)s"')
  (rc, out) = run(dark_template % parameters)
  if rc:
    for line in out.splitlines():
      print line
    print 'dark.exe returned %d' % rc
    return rc

  candle_template = ('"%(wix_path)s\\candle" ' +
                   '-nologo ' +
                     '"%(intermediate_dir)s/%(basename)s.wxs" ' +
                     '-o "%(intermediate_dir)s/%(basename)s.wixobj" ' +
                     '-ext "%(wix_path)s\\WixFirewallExtension.dll"')
  (rc, out) = run(candle_template % parameters)
  if rc:
    for line in out.splitlines():
      print line
    print 'candle.exe returned %d' % rc
    return rc

  light_template = ('"%(wix_path)s\\light" ' +
                   '-nologo ' +
                    '"%(intermediate_dir)s/%(basename)s.wixobj" ' +
                    '-o "%(output)s" ' +
                    '-ext "%(wix_path)s\\WixFirewallExtension.dll" ' +
                    '-sw1076 ')
  (rc, out) = run(light_template % parameters)
  if rc:
    for line in out.splitlines():
      print line
    print 'candle.exe returned %d' % rc
    return rc

  return 0

if __name__ == "__main__":
  sys.exit(main())
