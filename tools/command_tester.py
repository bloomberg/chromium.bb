#!/usr/bin/python
# Copyright (c) 2011 The Native Client Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.


"""Simple testing harness for running commands and checking expected output.

This harness is used instead of shell scripts to ensure windows compatibility

"""


# python imports
import getopt
import os
import sys

# local imports
import test_lib

GlobalPlatform=None  # for pychecker, initialized in ProcessOptions
GlobalReportStream = [sys.stdout]
GlobalSettings = {}
GlobalSigType = ['normal']

# Hook print to we can print to both stdout and a file
def Print(message):
  for s in GlobalReportStream:
    print >>s, message



def Banner(message):
  Print('=' * 70)
  print(message)
  print('=' * 70)


def DifferentFromGolden(actual, golden, output_type, fail_msg):
  """Compares actual output against golden output.

  If there are any differences, output an error message (to stdout) with
  appropriate banners.

  Args:
    actual: actual output from the program under test, as a single
      string.

    golden: expected output from the program under test, as a single
      string.

    output_type: the name / title for the output type being compared.
      Used in banner output.

    fail_msg:  additional failure output message, printed at the end.

  Returns:
    True when there is a difference, False otherwise.
  """

  diff = list(test_lib.DiffStringsIgnoringWhiteSpace(golden, actual))
  diff = '\n'.join(diff)
  if diff:
    Banner('Error %s diff found' % output_type)
    Print(diff)
    Banner('Potential New Golden Output')
    Print(actual)
    Print(fail_msg)
    return True
  return False


def ResetGlobalSettings():
  global GlobalSettings
  GlobalSettings = {
      'exit_status': 0,
      'osenv': '',

      'arch': None,
      'subarch': None,

      # An environment description that should include all factors that may
      # affect tracked performance. Used to compare different environments.
      'perf_env_description': None,

      # Track total time taken for the command: '0' or '1'.
      'track_cmdtime': '0',

      'name': None,
      'report': None,

      'stdin': None,
      'log_file': None,

      'stdout_golden': None,
      'stderr_golden': None,
      'log_golden': None,

      # This option must be '1' for the output to be captured, for checking
      # against golden files, special exit_status signals, etc.
      # When this option is '0', stdout and stderr will be streamed out.
      'capture_output': '1',

      'filter_regex': None,
      'filter_inverse': False,
      'filter_group_only': False,

      'time_warning': 0,
      'time_error': 0,

      'run_under': None,
  }


def FailureMessage():
  return '%s: FAILED' % os.path.basename(GlobalSettings['name'])

def SuccessMessage():
  return '%s: PASSED' % os.path.basename(GlobalSettings['name'])

def LogPerfResult(graph_name, trace_name, value, units):
  # NOTE: This RESULT message is parsed by Chrome's perf graph generator.
  Print('RESULT %s: %s= %s %s' %
        (graph_name, trace_name, value, units))

def PrintTotalTime(total_time):
  if int(GlobalSettings['track_cmdtime']):
    LogPerfResult(os.path.basename(GlobalSettings['name']),
                  'TOTAL_' + GlobalSettings['perf_env_description'],
                  '%f' % total_time,
                  'secs')
  else:
    Print('Test %s took %f secs' % (GlobalSettings['name'], total_time))


# Mac OS X returns SIGBUS in most of the cases where Linux returns
# SIGSEGV, except for actual x86 segmentation violations.
status_map = {
    'sigabrt' : {
        'linux2': -6, # SIGABRT
        'darwin': -6, # SIGABRT
        'cygwin': -6, # SIGABRT
        'win32':  -6, # SIGABRT
        'win64':  -6, # SIGABRT
        },
    'sigpipe': {
        'linux2': -13, # SIGPIPE
        'darwin': -13, # SIGPIPE
        'cygwin': -1073741819, # -0x3ffffffb or 0xc0000005
        'win32':  -1073741819, # -0x3ffffffb or 0xc0000005
        'win64':  -1073741819, # -0x3ffffffb or 0xc0000005
        },
    'sigill' : {
        'linux2': -11, # SIGSEGV
        'darwin': -11, # SIGBUS
        'cygwin': -1073741819, # -0x3ffffffb or 0xc0000005
        'win32':  -1073741819, # -0x3ffffffb or 0xc0000005
        'win64':  -1073741819, # -0x3ffffffb or 0xc0000005
        },
    'segfault': {
        'linux2': -11, # SIGSEGV
        'darwin': -10, # SIGBUS
        'cygwin': -1073741819, # -0x3ffffffb or 0xc0000005
        'win32':  -1073741819, # -0x3ffffffb or 0xc0000005
        'win64':  -1073741819, # -0x3ffffffb or 0xc0000005
        },
    'sigsegv_or_equivalent': {
        'linux2': -11, # SIGSEGV
        'darwin': -11, # SIGSEGV
        'cygwin': -1073741819, # -0x3ffffffb or 0xc0000005
        'win32':  -1073741819, # -0x3ffffffb or 0xc0000005
        'win64':  -1073741819, # -0x3ffffffb or 0xc0000005
        },
    'untrusted_sigill' : {
        'linux2': -11, # SIGSEGV
        'darwin': -11, # SIGBUS
        'cygwin': -1073741819, # -0x3ffffffb or 0xc0000005
        'win32':  [ -1073741819, # -0x3ffffffb or 0xc0000005
                    -1073741674, # -0x3fffff6a or 0xc0000096
                   ],
        'win64':  -4,
        },
    'untrusted_segfault': {
        'linux2': -11, # SIGSEGV
        'darwin': -10, # SIGBUS
        'cygwin': -1073741819, # -0x3ffffffb or 0xc0000005
        'win32':  -1073741819, # -0x3ffffffb or 0xc0000005
        'win64':  -11,
        },
    'untrusted_sigsegv_or_equivalent': {
        'linux2': -11, # SIGSEGV
        'darwin': -11, # SIGSEGV
        'cygwin': -1073741819, # -0x3ffffffb or 0xc0000005
        'win32':  [ -1073741819, # -0x3ffffffb or 0xc0000005
# TODO(khim): remove this when cause of STATUS_PRIVILEGED_INSTRUCTION will be
# known.  This only happens on win32 with glibc which is also suspicious.
# See: http://code.google.com/p/nativeclient/issues/detail?id=1689
                    -1073741674, # -0x3fffff6a or 0xc0000096
                   ],
        'win64':  -11,
        },
    'hybrid_log_fatal': {
        'linux2': {
            'normal': [ 1,],
            'untrusted': [ -11 ],
            }, # newlib vs glibc: exit 1 or signal 11
        'darwin': 1,
        'cygwin': 1,
        'win32': 1,
        'win64': 1,
        },
    'trusted_sigabrt' : {
        'linux2': -6, # SIGABRT
        'darwin': -6, # SIGABRT
        'cygwin': -6, # SIGABRT
        'win32':  -6, # SIGABRT
        'win64':  -6, # SIGABRT
        },
    'trusted_sigill' : {
        'linux2': -11, # SIGSEGV
        'darwin': -11, # SIGBUS
        'cygwin': -4, # SIGILL
        'win32':  -4, # SIGILL
        'win64':  -4, # SIGILL
        },
    'trusted_segfault': {
        'linux2': -11, # SIGSEGV
        'darwin': -10, # SIGBUS
        'cygwin': -11, # SIGSEGV
        'win32': -11, # SIGSEGV
        'win64': -11, # SIGSEGV
        },
    'trusted_sigsegv_or_equivalent': {
        'linux2': -11, # SIGSEGV
        'darwin': -11, # SIGSEGV
        'cygwin': -11, # SIGSEGV
        'win32': -11, # SIGSEGV
        'win64': -11, # SIGSEGV
        },
    }

def GetSigType(sig):
  if (type(sig)==type('string')):
    if (sig[:8]=='trusted_'):
      return ['trusted']

    if (sig[:10]=='untrusted_'):
      return ['untrusted']

    if (sig[:7]=='hybrid_'):
      return ['normal', 'untrusted']

  return ['normal']


def MassageExitStatus(v):
  global GlobalPlatform
  if v in status_map:
    return status_map[v][GlobalPlatform]
  else:
    return int(v)


def ProcessOptions(argv):
  global GlobalPlatform
  global GlobalSigType

  """Process command line options and return the unprocessed left overs."""
  ResetGlobalSettings()
  try:
    opts, args = getopt.getopt(argv, '', [x + '='  for x in GlobalSettings])
  except getopt.GetoptError, err:
    Print(str(err))  # will print something like 'option -a not recognized'
    sys.exit(-1)

  for o, a in opts:
    # strip the leading '--'
    option = o[2:]
    assert option in GlobalSettings
    if option == 'exit_status':
      GlobalSettings[option] = a
    elif type(GlobalSettings[option]) == int:
      GlobalSettings[option] = int(a)
    else:
      GlobalSettings[option] = a

  if (sys.platform == 'win32') and (GlobalSettings['subarch'] == '64'):
    GlobalPlatform = 'win64'
  else:
    GlobalPlatform = sys.platform

  GlobalSigType = GetSigType(GlobalSettings['exit_status'])
  # In win32, we can't catch a signal, so it always appears as a 'normal' exit
  if GlobalPlatform == 'win32' and GlobalSigType == ['untrusted']:
    GlobalSigType = ['normal']
  # return the unprocessed options, i.e. the command
  return args


# Parse output for signal type and number
#
# The '** Signal' output is from the nacl signal handler code and is
# only appropriate for non-qemu runs; when the emulator is used, the
# 2nd match with 'qemu: uncaught target signal ddd' yields the signal
# number.
#
# Since it is possible for there to be an output race with another
# thread, or additional output due to atexit functions, we scan the
# output in reverse order for the signal signature.
#
# TODO(noelallen,bsy): when nacl_signal testing is enabled for arm,
# remove/fix the explicit qemu match code below.
#
def ExitStatusInfo(stderr):
  sigNum = 0
  sigType = 'normal'

  lines = stderr.splitlines()

  # Scan for signal msg in reverse order
  for curline in reversed(lines):
    words = curline.split()
    if len(words) > 4:
      if words[0] == '**' and words[1] == 'Signal':
        sigNum = -int(words[2])
        sigType= words[4]
        break
      if (words[0] == 'qemu:' and words[1] == 'uncaught' and
          words[2] == 'target' and words[3] == 'signal'):
        sigNum = -int(words[4])
        break
  return sigNum, sigType

# On linux return codes are 8 bit. So -n = 256 + (-n) (eg: 243 = -11)
# python does not understand this 8 bit wraparound.
# Thus, we convert the desired val and returned val if negative to postive
# before comparing.
def ExitStatusIsOK(expected, actual, stderr):
  global GlobalSigType

  sigNum, sigType = ExitStatusInfo(stderr)

  # If scanning the output reveals an exit status of a type unexpected
  if sigType not in GlobalSigType:
    return False

  if isinstance(expected, dict):
    if sigType not in expected:
      return False
    expected = expected[sigType]
  return ExitStatusIsOKNoSignals(expected, actual)

# Version that skips checking signals.
def ExitStatusIsOKNoSignals(expected, actual):
  # If the expected value is a list
  if isinstance(expected, list):
    for e in expected:
      a = actual
      if e < 0:
        e = e % 256
        a = a % 256
      if e == a:
        return True
    return False
  else:
    if expected < 0:
      expected = expected % 256
      actual = actual % 256

    val = expected == actual
    return val

def CheckExitStatusWithSignals(failed,
                               req_status,
                               exit_status,
                               stdout,
                               stderr):
  exitOk = ExitStatusIsOK(req_status, exit_status, stderr)
  if exitOk and not failed:
    return True
  if failed:
    Print('Command failed')
  else:
    sigNum, sigType = ExitStatusInfo(stderr)
    Print('\nERROR: Command returned %s %s, expecting %s %s for %s' %
          (sigType, str(exit_status),
           str(GlobalSigType), str(req_status), GlobalPlatform))
    Banner('Stdout')
    Print(stdout)
    Banner('Stderr')
    Print(stderr)
    Print(FailureMessage())
  return False

def CheckExitStatusNoSignals(failed,
                             req_status,
                             exit_status):
  exitOk = ExitStatusIsOKNoSignals(req_status, exit_status)
  if exitOk and not failed:
    return True
  if failed:
    Print('Command failed')
  else:
    Print('\nERROR: Command returned %s, expecting %s for %s' %
          (str(exit_status),
           str(req_status),
           GlobalPlatform))
    Print(FailureMessage())
  return False

def CheckTimeBounds(total_time):
  if GlobalSettings['time_error']:
    if total_time > GlobalSettings['time_error']:
      Print('ERROR: should have taken less than %f secs' %
            (GlobalSettings['time_error']))
      Print(FailureMessage())
      return False

  if GlobalSettings['time_warning']:
    if total_time > GlobalSettings['time_warning']:
      Print('WARNING: should have taken less than %f secs' %
            (GlobalSettings['time_warning']))
  return True

def CheckGoldenOutput(stdout, stderr):
  for (stream, getter) in [
      ('stdout', lambda: stdout),
      ('stderr', lambda: stderr),
      ('log', lambda: open(GlobalSettings['log_file']).read()),
      ]:
    golden = stream + '_golden'
    if GlobalSettings[golden]:
      golden_data = open(GlobalSettings[golden]).read()
      actual = getter()
      if GlobalSettings['filter_regex']:
        actual = test_lib.RegexpFilterLines(GlobalSettings['filter_regex'],
                                            GlobalSettings['filter_inverse'],
                                            GlobalSettings['filter_group_only'],
                                            actual)
      if DifferentFromGolden(actual, golden_data, stream,
                             FailureMessage()):
        return False
  return True

def main(argv):
  global GlobalSigType
  global GlobalPlatform
  global GlobalReportStream
  command = ProcessOptions(argv)

  if GlobalSettings['report']:
    GlobalReportStream.append(open(GlobalSettings['report'], 'w'))

  if not GlobalSettings['name']:
    GlobalSettings['name'] = command[0]

  if GlobalSettings['osenv']:
    Banner('setting environment')
    # BUG(robertm): , is a legitimate character for an environment variable
    # value.
    env_vars = GlobalSettings['osenv'].split(',')
  else:
    env_vars = []
  for env_var in env_vars:
    key, val = env_var.split('=', 1)
    Print('[%s] = [%s]' % (key, val))
    os.putenv(key, val)

  stdin_data = ''
  if GlobalSettings['stdin']:
    stdin_data = open(GlobalSettings['stdin'])

  if GlobalSettings['log_file']:
    try:
      os.unlink(GlobalSettings['log_file'])  # might not pre-exist
    except OSError:
      pass

  run_under = GlobalSettings['run_under']
  if run_under:
    command = run_under.split(',') + command

  Banner('running %s' % str(command))
  # print the command in copy-and-pastable fashion
  print " ".join(env_vars + command)

  if not int(GlobalSettings['capture_output']):
    # We are only blurting out the stdout and stderr, not capturing it
    # for comparison, etc.
    assert (not GlobalSettings['stdout_golden']
            and not GlobalSettings['stderr_golden']
            and not GlobalSettings['log_golden']
            and not GlobalSettings['filter_regex']
            and not GlobalSettings['filter_inverse']
            and not GlobalSettings['filter_group_only']
            )
    # If python ever changes popen.stdout.read() to not risk deadlock,
    # we could stream and capture, and use RunTestWithInputOutput instead.
    (total_time, exit_status, failed) = test_lib.RunTestWithInput(command,
                                                                  stdin_data)
    PrintTotalTime(total_time)
    req_status = MassageExitStatus(GlobalSettings['exit_status'])
    if not CheckExitStatusNoSignals(failed,
                                    req_status,
                                    exit_status):
      return -1
  else:
    (total_time, exit_status,
     failed, stdout, stderr) = test_lib.RunTestWithInputOutput(
         command, stdin_data)
    PrintTotalTime(total_time)
    req_status = MassageExitStatus(GlobalSettings['exit_status'])
    if not CheckExitStatusWithSignals(failed,
                                       req_status,
                                       exit_status,
                                       stdout,
                                       stderr):
      return -1
    if not CheckGoldenOutput(stdout, stderr):
      return -1

  if not CheckTimeBounds(total_time):
    return -1

  Print(SuccessMessage())
  return 0


if __name__ == '__main__':
  sys.exit(main(sys.argv[1:]))
