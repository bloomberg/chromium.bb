#!/usr/bin/python
# Copyright 2010 The Native Client Authors.  All rights reserved.
# Use of this source code is governed by a BSD-style license that can
# be found in the LICENSE file.

"""Simple script for running tests on binaries that were built using GYP.

Possible flags include: --config (Debug/Release)
                        --platform (win32/win63/linux32/linux64/mac32)
                        --size (small/medium/large)

"""

import getopt
import os
import sys
sys.path.append('./common')
sys.path.append('./tools')

import command_tester

# Each test is a list of 4 items:
# 1. binary to run (string),
# 2. options - test arguments, such as stdin (list of strings),
# 3. extras - additional arguments to the binary (list of strings),
# 4. test size (string)
# Simple tests can be configured here. More complex tests can be added later -
# see AddNdisTest below
TESTS = [
    ['service_runtime_tests', [], [], 'small'],
    ['nacl_sync_cond_test', [], [], 'small'],
]

# If a specific test should not run on a specific platform, add it to this list
EXCEPTIONS = [
    # Example entry: ['service_runtime_tests', 'win64'],
]

# ----------------------------------------------------------
TEST_TIME_THRESHOLD = {
    'small': 2,
    'medium': 10,
    'large': 60,
    'huge': 1800,
    }

# Default values for various settings, can be changed using command line
# arguments or in the code.
GlobalSettings = {
    'config': 'Debug',
    'platform': None,
    'exe_suffix': '',
    'out_suffix': '.out',
    'size': None,
}


# ----------------------------------------------------------
# TODO(gregoryd): move test definitions into the appropriate locations
def AddNcdisTest():
  name = 'ncdis'
  native_client_dir = GetLocalPath()
  stdin_path = os.path.join(native_client_dir, 'src', 'trusted',
                            'validator_x86', 'testdata', '32',
                            'ncdis_test.input')
  stdout_path = os.path.join(native_client_dir, 'src', 'trusted',
                             'validator_x86', 'testdata', '32',
                             'ncdis_test.input')
  options = ['--stdin=%s' % stdin_path,
             '--stdout_golden=%s' % stdout_path]
  extras = ['-self_document',
            '-commands=-']
  TESTS.append([name, options, extras, 'small'])


def AddTests():
  AddNcdisTest()


# ----------------------------------------------------------
def Banner(text):
  print '=' * 70
  print text
  print '=' * 70


# ----------------------------------------------------------
# Utility functions
def GetLocalPath():
  head, tail = os.path.split(os.path.abspath(__file__))
  return head


def GetBinaryPath():
  """Returns the absolute path for the GYP output directory."""
  native_client_directory = GetLocalPath()
  # remove the 32/64 information
  platform = GlobalSettings['platform'][:-2]
  if platform == 'win':
    path = os.path.join(native_client_directory, 'build')
  elif platform == 'linux':
    path = os.path.join(native_client_directory, '..', 'out')
  elif platform == 'mac':
    path = os.path.join(native_client_directory, '..', 'xcodebuild')
  return os.path.join(path, GlobalSettings['config'])


def GetTestBinaryPath(test_name):
  return os.path.join(GetBinaryPath(),
                      test_name + GlobalSettings['exe_suffix'])


def GetTestOutputPath(test_name):
  """Return the default output filename."""
  return os.path.join(GetBinaryPath(),
                      test_name + GlobalSettings['out_suffix'])


# ----------------------------------------------------------
def CommandTest(test_name, options=[], extra=[], size='small'):
  output = GetTestOutputPath(test_name)
  binary = GetTestBinaryPath(test_name)

  max_time = TEST_TIME_THRESHOLD[size]

  script_flags = ['--name='+ binary,
                  '--report='+ output,
                  '--time_warning='+ str(max_time),
                  '--time_error='+ str(10 * max_time),
                 ]

  for o in options:
    if o != '':
      script_flags.append(o)

  command_tester.main(script_flags + [binary] + extra)


def ValidatePlatform():
  """ We need to verify that if the platform was specified on the command line,
      it matches the host OS. """
  platform = 'unknown'
  if sys.platform in ('cygwin', 'win32'):
    platform = 'win'
    GlobalSettings['exe_suffix'] = '.exe'
  if sys.platform in ('linux', 'linux2'):
    platform = 'linux'
  if sys.platform == 'darwin':
    platform = 'mac'
  if sys.maxsize == 2**31-1:
    platform += '32'
  else:
    platform += '64'

  if not GlobalSettings['platform']:
    # platform was not provided on the command line
    GlobalSettings['platform'] = platform
  elif GlobalSettings['platform'] != platform:
    if platform != 'linux64' or GlobalSettings['platform'] != 'linux32':
      # we allow running 32-bit tests on 64-bit Linux, otherwise it's an error
      print('Cannot run %s tests on %s' %
            (GlobalSettings['platform'], platform))
      sys.exit(-1)


def ProcessOptions(argv):
  """Process command line options and return the unprocessed left overs."""
  try:
    opts, args = getopt.getopt(argv, '', [x + '='  for x in GlobalSettings])
  except getopt.GetoptError, err:
    Print(str(err))  # will print something like 'option -a not recognized'
    sys.exit(-1)

  for o, a in opts:
    # strip the leading '--'
    option = o[2:]
    assert option in GlobalSettings
    if type(GlobalSettings[option]) == int:
      GlobalSettings[option] = int(a)
    else:
      GlobalSettings[option] = a

  # Validate platform settings
  ValidatePlatform()

  # Validate size
  if (GlobalSettings['size'] and
      not GlobalSettings['size'] in TEST_TIME_THRESHOLD.keys()):
    print 'Unknown test size: %s' % GlobalSettings['size']
    sys.exit(-1)

  # Validate config
  if not GlobalSettings['config'] in ['Debug', 'Release']:
    print 'Unknown config: %s' % GlobalSettings['config']
    sys.exit(-1)

  # return the unprocessed options, i.e. the command
  return args


def main(argv):
  ProcessOptions(argv)
  AddTests()
  for test in TESTS:
    try:
      # Don't run the test if the [test, platform] combination is in EXCEPTIONS
      i = EXCEPTIONS.index([test[0], GlobalSettings['platform']])
    except ValueError:
      # Run only tests of the right size
      if not GlobalSettings['size'] or test[3] == GlobalSettings['size']:
        CommandTest(test[0], test[1], test[2], test[3])

if __name__ == '__main__':
  sys.exit(main(sys.argv[1:]))
