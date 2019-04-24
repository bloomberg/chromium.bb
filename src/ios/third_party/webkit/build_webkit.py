#!/usr/bin/env python
# Copyright 2019 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import optparse
import os
import subprocess
import sys

def main():
  usage = ('%prog [options]\n'
           'Invokes build-webkit with the given options.')
  parser = optparse.OptionParser(usage=usage)
  parser.add_option('-o', '--output_dir',
                    help='Output directory for build products.')
  (opts, args) = parser.parse_args()

  if not opts.output_dir:
    print >>sys.stderr, '--output_dir is required.'
    return 1

  # TODO(crbug.com/934252): "-jobs 2" is passed along to the underlying
  # xcodebuild invocation and restricts xcodebuild to two simultaneous
  # jobs. This is intended to prevent overloading the machine, because ninja
  # will already be spawning a large number of jobs in parallel with xcodebuild,
  # but it causes the webkit build to run very slowly. Find a way to increase
  # the parallelism here.
  command = ['src/Tools/Scripts/build-webkit', '--debug', '--ios-simulator',
             '-quiet', '-jobs', '2']

  env = {'WEBKIT_OUTPUTDIR': opts.output_dir}
  cwd = os.path.dirname(os.path.realpath(__file__))
  proc = subprocess.Popen(command, cwd=cwd, env=env)
  proc.communicate()
  return proc.returncode

if __name__ == '__main__':
  sys.exit(main())
