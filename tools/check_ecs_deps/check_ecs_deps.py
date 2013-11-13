#!/usr/bin/env python
# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

''' Verifies that builds of the embedded content_shell do not included
unnecessary dependencies.'''

import getopt
import os
import re
import string
import subprocess
import sys
import optparse

kUndesiredLibraryList = [
  'libcairo',
  'libpango',
  'libglib',
]

binary_target = 'content_shell'

def _main():
  parser = optparse.OptionParser(
      "usage: %prog -b <dir> --target <Debug|Release>")
  parser.add_option("-b", "--build-dir",
                    help="the location of the compiler output")
  parser.add_option("--target", help="Debug or Release")

  options, args = parser.parse_args()
  # Bake target into build_dir.
  if options.target and options.build_dir:
    assert (options.target !=
            os.path.basename(os.path.dirname(options.build_dir)))
    options.build_dir = os.path.join(os.path.abspath(options.build_dir),
                                     options.target)

  if options.build_dir != None:
      target = os.path.join(options.build_dir, binary_target)
  else:
    target = binary_target

  forbidden_regexp = re.compile(string.join(kUndesiredLibraryList, '|'))
  success = 0

  p = subprocess.Popen(['ldd', target], stdout=subprocess.PIPE,
      stderr=subprocess.PIPE)
  out, err = p.communicate()

  if err != '':
    print "Failed to execute ldd to analyze dependencies for " + target + ':'
    print '    ' + err
    print "FAILED\n"
    return 1

  if out == '':
    print "No output to scan for forbidden dependencies?\n"
    print "\nFAILED\n"
    return 1

  success = 1
  deps = string.split(out, '\n')
  for d in deps:
      if re.search(forbidden_regexp, d) != None:
        success = 0
        print "Forbidden library: " +  d

  if success == 1:
    print "\nSUCCESS\n"
    return 0
  else:
    print "\nFAILED\n"
    return 1

if __name__ == "__main__":
  # handle arguments...
  # do something reasonable if not run with one...
  sys.exit(_main())
