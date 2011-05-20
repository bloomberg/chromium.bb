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
    ['format_string_test', [], [], 'small'],
    ['nacl_sync_cond_test', [], [], 'small'],
    ['env_cleanser_test', [], [], 'small'],
]

# If a specific test should not run on a specific platform, add it to this list
EXCEPTIONS = [
    # Example entry: ['service_runtime_tests', 'win64'],
]

# ----------------------------------------------------------
TEST_TIME_THRESHOLD = {
    'small': 2,
    'small_tests_arm_hw_only': 2,
    'medium': 10,
    'large': 60,
    'large_tests_arm_hw_only':60,
    'huge': 1800,
    }

# Default values for various settings, can be changed using command line
# arguments or in the code.
GlobalSettings = {
    'config': 'Debug',
    'bits': 32,
    'platform': None,
    'exe_suffix': '',
    'out_suffix': '.out',
    'size': None,
}


def AddServiceRuntimeTests():
  # Service_runtime_tests itself is in the initialized TESTS
  if GlobalSettings['platform'].startswith('arm'):
    params = [ '-n', '512', '-m', '2']
    TESTS.append(['gio_shm_unbounded_test', [], [], 'large_tests_arm_hw_only'])
  else:
    params = []
    TESTS.append(['gio_shm_unbounded_test', [], [], 'small'])
  TESTS.append(['gio_shm_test', [], params, 'large'])

  # Check tests
  abort_exit_status = ['--exit_status=17'] # magic, see nacl_check_test.c
  TESTS.append(['nacl_check_test', [], ['-C'], 'small'])

  # Death test
  TESTS.append(['nacl_check_test', abort_exit_status,
               ['-c'], 'small'])
  if GlobalSettings['config'] == 'Debug':
    TESTS.append(['nacl_check_test', abort_exit_status, ['-d'], 'small'])
  else:
    TESTS.append(['nacl_check_test', [], ['-d'], 'small'])
  TESTS.append(['nacl_check_test', [], ['-s', '0', '-C'], 'small'])
  TESTS.append(['nacl_check_test', abort_exit_status,
               ['-s', '0', '-c'], 'small'])
  TESTS.append(['nacl_check_test', [], ['-s', '0', '-d'], 'small'])
  TESTS.append(['nacl_check_test', abort_exit_status,
               ['-s', '1', '-d'], 'small'])
  # Mac does not support thread local storage via "__thread" so do not run this
  # test on Mac
  if not GlobalSettings['platform'].startswith('mac'):
    # Note that this test hangs in pthread_join() on ARM QEMU.
    if GlobalSettings['platform'].startswith('arm'):
      size = 'small_tests_arm_hw_only'
    else:
      size = 'small'
    TESTS.append(['nacl_tls_unittest', [], [], size])
  # TODO(issue #540): Make it work on windows.
  if not GlobalSettings['platform'].startswith('win'):
    segfault = str(command_tester.MassageExitStatus('segfault'))
    TESTS.append(['sel_ldr_thread_death_test', ['--exit_status=' + segfault],
                 [], 'medium'])


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

  return command_tester.main(script_flags + [binary] + extra)


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
  if sys.maxint == 2**31-1:
    platform += '32'
  else:
    platform += '64'

  is_arm = 0
  try:
    gyp_defines = os.environ['GYP_DEFINES']
    if gyp_defines.find('target_arch=arm') != -1:
      is_arm = 1
  except:
    print('GYP_DEFINES is not set')
  if (is_arm):
    print('ARM is not supported yet')
    sys.exit(0)

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
    print '%s' % str(err)  # Something like 'option -a not recognized'.
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
  errors = 0
  for test in TESTS:
    try:
      # Don't run the test if the [test, platform] combination is in EXCEPTIONS
      EXCEPTIONS.index([test[0], GlobalSettings['platform']])
    except ValueError:
      # Run only tests of the right size
      if not GlobalSettings['size'] or test[3] == GlobalSettings['size']:
        errors += CommandTest(test[0], test[1], test[2], test[3])
  return errors

if __name__ == '__main__':
  sys.exit(main(sys.argv[1:]))
