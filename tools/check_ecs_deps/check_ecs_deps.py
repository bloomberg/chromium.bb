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
#  'libglib',  # TODO(spang) Stop depending on this.
]

kAllowedLibraryList = [
  'linux-vdso',
  'libfreetype',
  'librt',
  'libdl',
  'libgobject-2.0',
  'libnss3',
  'libnssutil3',
  'libsmime3',
  'libplc4',
  'libnspr4',
  'libfontconfig',
  'libdrm',
  'libasound',
  'libexpat',
  # 'libudev', # TODO(rjkroege) Decide about this one.
  'libstdc++',
  'libm',
  'libgcc_s',
  'libpthread',
  'libc',
  'libz',
  'libffi',
  'libpcre',
  'libplds4',
]

binary_target = 'content_shell'

def stdmsg(final, errors):
  if errors:
    for message in errors:
      print message

def bbmsg(final, errors):
  if errors:
    for message in errors:
      print '@@@STEP_TEXT@%s@@@' % message
  if final:
    print '\n@@@STEP_%s@@@' % final


def _main():
  output = {
    'message': lambda x: stdmsg(None, x),
    'fail': lambda x: stdmsg('FAILED', x),
    'warn': lambda x: stdmsg('WARNING', x),
    'abend': lambda x: stdmsg('FAILED', x),
    'ok': lambda x: stdmsg('SUCCESS', x),
  }

  parser = optparse.OptionParser(
      "usage: %prog -b <dir> --target <Debug|Release>")
  parser.add_option("", "--annotate", dest='annotate', action='store_true',
      default=False, help="include buildbot annotations in output")
  parser.add_option("", "--noannotate", dest='annotate', action='store_false')
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

  if options.annotate:
    output = {
      'message': lambda x: bbmsg(None, x),
      'fail': lambda x: bbmsg('FAILURE', x),
      'warn': lambda x: bbmsg('WARNINGS', x),
      'abend': lambda x: bbmsg('EXCEPTIONS', x),
      'ok': lambda x: bbmsg(None, x),
    }

  forbidden_regexp = re.compile(string.join(map(re.escape,
                                                kUndesiredLibraryList), '|'))
  mapping_regexp = re.compile(r"\s*([^/]*) => ")
  blessed_regexp = re.compile(r"(%s)[-0-9.]*\.so" % string.join(map(re.escape,
      kAllowedLibraryList), '|'))
  success = 0
  warning = 0

  p = subprocess.Popen(['ldd', target], stdout=subprocess.PIPE,
      stderr=subprocess.PIPE)
  out, err = p.communicate()

  if err != '':
    output['abend']([
      'Failed to execute ldd to analyze dependencies for ' + target + ':',
      '    ' + err,
    ])
    return 1

  if out == '':
    output['abend']([
      'No output to scan for forbidden dependencies.'
    ])
    return 1

  success = 1
  deps = string.split(out, '\n')
  for d in deps:
      libmatch = mapping_regexp.match(d)
      if libmatch:
        lib = libmatch.group(1)
        if forbidden_regexp.search(lib):
          success = 0
          output['message'](['Forbidden library: ' +  lib])
        if not blessed_regexp.match(lib):
          warning = 1
          output['message'](['Unexpected library: ' +  lib])

  if success == 1:
    if warning == 1:
      output['warn'](None)
    else:
      output['ok'](None)
    return 0
  else:
    output['fail'](None)
    return 1

if __name__ == "__main__":
  # handle arguments...
  # do something reasonable if not run with one...
  sys.exit(_main())
