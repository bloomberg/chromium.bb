# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import argparse
import os
import shutil
import sys

def touch(fname):
  if os.path.exists(fname):
    os.utime(fname, None)
  else:
    open(fname, 'a').close()

def main():
  """Command line utility to copy python binary modules to the correct package
  hierarchy.
  """
  parser = argparse.ArgumentParser(
      description='Copy python binary modules to the correct package '
                  'hierarchy.')
  parser.add_argument('timestamp', help='The timetsamp file.')
  parser.add_argument('lib_dir', help='The directory containing the modules')
  parser.add_argument('destination_dir',
                      help='The destination directory of the module')
  parser.add_argument('mappings', nargs='+',
                      help='The mapping from module to library.')
  opts = parser.parse_args()

  if not os.path.exists(opts.destination_dir):
    os.makedirs(opts.destination_dir)

  for mapping in opts.mappings:
    [module, library] = mapping.split('=')
    shutil.copy(os.path.join(opts.lib_dir, library),
                os.path.join(opts.destination_dir, module))

  touch(opts.timestamp)

if __name__ == '__main__':
  main()
