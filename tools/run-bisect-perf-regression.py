#!/usr/bin/env python
# Copyright (c) 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Run Performance Test Bisect Tool

This script is used by a trybot to run the src/tools/bisect-perf-regression.py
script with the parameters specified in run-bisect-perf-regression.cfg. It will
check out a copy of the depot in a subdirectory 'bisect' of the working
directory provided, and run the bisect-perf-regression.py script there.

"""

import imp
import optparse
import os
import subprocess
import sys


def LoadConfigFile(path_to_file):
  """Attempts to load the file 'run-bisect-perf-regression.cfg' as a module
  and grab the global config dict.

  Args:
    path_to_file: Path to the run-bisect-perf-regression.cfg file.

  Returns:
    The config dict which should be formatted as follows:
    {'command': string, 'good_revision': string, 'bad_revision': string
     'metric': string}.
    Returns None on failure.
  """
  try:
    local_vars = {}
    execfile(os.path.join(path_to_file, 'run-bisect-perf-regression.cfg'),
             local_vars)

    return local_vars['config']
  except:
    return None


def RunBisectionScript(config, working_directory, path_to_file):
  """Attempts to execute src/tools/bisect-perf-regression.py with the parameters
  passed in.

  Args:
    config: A dict containing the parameters to pass to the script.
    working_directory: A working directory to provide to the
      bisect-perf-regression.py script, where it will store it's own copy of
      the depot.
    path_to_file: Path to the bisect-perf-regression.py script.

  Returns:
    0 on success, otherwise 1.
  """

  cmd = ['python', os.path.join(path_to_file, 'bisect-perf-regression.py'),
         '-c', config['command'],
         '-g', config['good_revision'],
         '-b', config['bad_revision'],
         '-m', config['metric'],
         '--working_directory', working_directory,
         '--output_buildbot_annotations']

  return_code = subprocess.call(cmd)

  if return_code:
    print 'Error: bisect-perf-regression.py returned with error %d' %\
        return_code
    print

  return return_code


def main():

  usage = ('%prog [options] [-- chromium-options]\n'
           'Used by a trybot to run the bisection script using the parameters'
           ' provided in the run-bisect-perf-regression.cfg file.')

  parser = optparse.OptionParser(usage=usage)
  parser.add_option('-w', '--working_directory',
                    type='str',
                    help='A working directory to supply to the bisection '
                    'script, which will use it as the location to checkout '
                    'a copy of the chromium depot.')
  (opts, args) = parser.parse_args()

  if not opts.working_directory:
    print 'Error: missing required parameter: --working_directory'
    print
    parser.print_help()
    return 1

  path_to_file = os.path.abspath(os.path.dirname(sys.argv[0]))

  config = LoadConfigFile(path_to_file)
  if not config:
    print 'Error: Could not load config file.'
    print
    return 1

  return RunBisectionScript(config, opts.working_directory, path_to_file)


if __name__ == '__main__':
  sys.exit(main())
