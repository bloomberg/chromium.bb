#!/usr/bin/env python
# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Run 'candle' and 'light' to transform .wxs to .msi."""

from optparse import OptionParser
import os
import subprocess
import sys

def run(command, filter=None):
  popen = subprocess.Popen(
      command, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
  out, _ = popen.communicate()
  for line in out.splitlines():
    if filter and line.strip() != filter:
      print line
  return popen.returncode

def main():
  parser = OptionParser()
  parser.add_option('--wix_path', dest='wix_path')
  parser.add_option('--version', dest='version')
  parser.add_option('--product_dir', dest='product_dir')
  parser.add_option('--intermediate_dir', dest='intermediate_dir')
  parser.add_option('--sas_dll_path', dest='sas_dll_path')
  parser.add_option('-d', dest='define_list', action='append')
  parser.add_option('--input', dest='input')
  parser.add_option('--output', dest='output')
  options, args = parser.parse_args()
  if args:
    parser.error("no positional arguments expected")
  parameters = dict(options.__dict__)

  parameters['basename'] = os.path.splitext(os.path.basename(options.input))[0]
  parameters['defines'] = '-d' + ' -d'.join(parameters['define_list'])

  common = (
      '-nologo '
      '-ext %(wix_path)s\\WixFirewallExtension.dll '
      '-ext %(wix_path)s\\WixUIExtension.dll '
      '-ext %(wix_path)s\\WixUtilExtension.dll '
      '-dVersion=%(version)s '
      '-dFileSource=%(product_dir)s '
      '-dIconPath=resources/chromoting.ico '
      '-dSasDllPath=%(sas_dll_path)s/sas.dll '
      '%(defines)s '
      )

  candle_template = ('%(wix_path)s\\candle ' +
                    common +
                    '-out %(intermediate_dir)s/%(basename)s.wixobj ' +
                    '%(input)s ')
  rc = run(candle_template % parameters, os.path.basename(parameters['input']))
  if rc:
    return rc

  light_template = ('%(wix_path)s\\light ' +
                    common +
                    '-cultures:en-us ' +
                    '-sw1076 ' +
                    '-out %(output)s ' +
                    '%(intermediate_dir)s/%(basename)s.wixobj ')
  rc = run(light_template % parameters)
  if rc:
    return rc

  return 0

if __name__ == "__main__":
  sys.exit(main())
