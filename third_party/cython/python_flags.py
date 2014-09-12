# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import argparse
import os
import sys

from distutils import sysconfig
from distutils.command import build_ext
from distutils.dist import Distribution
from distutils.extension import Extension

def main():
  """Command line utility to retrieve compilation options for python modules'
  """
  parser = argparse.ArgumentParser(
      description='Retrieves compilation options for python modules.')
  parser.add_argument('--gn',
                      help='Returns all values in a format suitable for gn',
                      action='store_true')
  parser.add_argument('--libraries', help='Returns libraries',
                      action='store_true')
  parser.add_argument('--includes', help='Returns includes',
                      action='store_true')
  parser.add_argument('--library_dirs', help='Returns library_dirs',
                      action='store_true')
  opts = parser.parse_args()

  ext = Extension('Dummy', [])
  b = build_ext.build_ext(Distribution())
  b.initialize_options()
  b.finalize_options()
  result = []
  if opts.libraries:
    libraries = b.get_libraries(ext)
    if sys.platform == 'darwin':
      libraries.append('python%s' % sys.version[:3])
    if not opts.gn and sys.platform in ['darwin', 'linux2']:
      # In case of GYP output for darwin and linux prefix all
      # libraries (if there are any) so the result can be used as a
      # compiler argument. GN handles platform-appropriate prefixing itself.
      libraries = ['-l%s' % library for library in libraries]
    result.extend(libraries)
  if opts.includes:
    result = result  + b.include_dirs
  if opts.library_dirs:
    if sys.platform == 'darwin':
      result.append('%s/lib' % sysconfig.get_config_vars('prefix')[0])

  if opts.gn:
    for x in result:
      print x
  else:
    print ''.join(['"%s"' % x for x in result])

if __name__ == '__main__':
  main()
