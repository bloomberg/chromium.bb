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
      # List of environment variables to set.
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

      # Script for processing output along with its arguments.
      'process_output': '',

      'time_warning': 0,
      'time_error': 0,

      'run_under': None,
  }

def StringifyList(lst):
  return ','.join(lst)

def DestringifyList(lst):
  # BUG(robertm): , is a legitimate character for an environment variable
  # value.
  return lst.split(',')

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


# On POSIX systems, exit() codes are 8-bit.  You cannot use exit() to
# make it look like the process was killed by a signal.  Instead,
# NaCl's signal handler encodes the signal number into the exit() code
# by returning with exit(-signum) or equivalently, exit((-signum) & 0xff).
# NaCl uses the same encoding on Windows.
def IndirectSignal(signum):
  return 256 - signum

# Mac OS X returns SIGBUS in most of the cases where Linux returns
# SIGSEGV, except for actual x86 segmentation violations.
status_map = {
    'sigabrt' : {
        # This is not used on Windows.
        'linux2': [-6], # SIGABRT
        'darwin': [-6], # SIGABRT
        },
    'naclabort_coverage' : {
        # This case is here because NaClAbort() behaves differently when
        # code coverage is enabled.
        # This is not used on Windows.
        'linux2': [IndirectSignal(6)], # SIGABRT
        'darwin': [IndirectSignal(6)], # SIGABRT
        },
    'sigpipe': {
        # This is not used on Windows because Windows does not have an
        # equivalent of SIGPIPE.
        'linux2': [-13], # SIGPIPE
        'darwin': [-13], # SIGPIPE
        },
    'untrusted_sigill' : {
        'linux2': [IndirectSignal(11)], # SIGSEGV
        'darwin': [IndirectSignal(11)], # SIGSEGV
        'win32':  [ -1073741819, # -0x3ffffffb or 0xc0000005
                    -1073741674, # -0x3fffff6a or 0xc0000096
                   ],
        'win64':  [IndirectSignal(4)], # SIGILL, as mapped by NaCl
        },
    'untrusted_segfault': {
        'linux2': [IndirectSignal(11)], # SIGSEGV
        'darwin': [IndirectSignal(10)], # SIGBUS
        'win32':  [-1073741819], # -0x3ffffffb or 0xc0000005
        'win64':  [IndirectSignal(11)],
        },
    'untrusted_sigsegv_or_equivalent': {
        'linux2': [IndirectSignal(11)], # SIGSEGV
        'darwin': [IndirectSignal(11)], # SIGSEGV
        'win32':  [ -1073741819, # -0x3ffffffb or 0xc0000005
# TODO(khim): remove this when cause of STATUS_PRIVILEGED_INSTRUCTION will be
# known.  This only happens on win32 with glibc which is also suspicious.
# See: http://code.google.com/p/nativeclient/issues/detail?id=1689
                    -1073741674, # -0x3fffff6a or 0xc0000096
                   ],
        'win64':  [IndirectSignal(11)],
        },
    'trusted_segfault': {
        'linux2': [IndirectSignal(11)], # SIGSEGV
        'darwin': [IndirectSignal(10)], # SIGBUS
        'win32':  [IndirectSignal(11)], # SIGSEGV
        'win64':  [IndirectSignal(11)], # SIGSEGV
        },
    'trusted_sigsegv_or_equivalent': {
        'linux2': [IndirectSignal(11)], # SIGSEGV
        'darwin': [IndirectSignal(11)], # SIGSEGV
        'win32':  [IndirectSignal(11)], # SIGSEGV
        'win64':  [IndirectSignal(11)], # SIGSEGV
        },
    }


def ProcessOptions(argv):
  global GlobalPlatform

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

  # return the unprocessed options, i.e. the command
  return args


# Parse output for signal type and number
#
# The '** Signal' output is from the nacl signal handler code.
#
# Since it is possible for there to be an output race with another
# thread, or additional output due to atexit functions, we scan the
# output in reverse order for the signal signature.
def GetNaClSignalInfoFromStderr(stderr):
  sigNum = 0
  sigType = 'normal'

  lines = stderr.splitlines()

  # Scan for signal msg in reverse order
  for curline in reversed(lines):
    words = curline.split()
    if len(words) > 4 and words[0] == '**' and words[1] == 'Signal':
      sigNum = -int(words[2])
      sigType = words[4]
      break
  return sigNum, sigType

def GetQemuSignalFromStderr(stderr, default):
  for line in reversed(stderr.splitlines()):
    # Look for 'qemu: uncaught target signal XXX'.
    words = line.split()
    if (len(words) > 4 and
        words[0] == 'qemu:' and words[1] == 'uncaught' and
        words[2] == 'target' and words[3] == 'signal'):
      return -int(words[4])
  return default

def CheckExitStatus(failed, req_status, exit_status, stdout, stderr):
  if req_status in status_map:
    expected_statuses = status_map[req_status][GlobalPlatform]
    if req_status.startswith('trusted_'):
      expected_sigtype = 'trusted'
    elif req_status.startswith('untrusted_'):
      expected_sigtype = 'untrusted'
    else:
      expected_sigtype = 'normal'
  else:
    expected_statuses = [int(req_status)]
    expected_sigtype = 'normal'

  # On 32-bit Windows, we cannot catch a signal that occurs in x86-32
  # untrusted code, so it always appears as a 'normal' exit, which
  # means that the signal handler does not print a message.
  if GlobalPlatform == 'win32' and expected_sigtype == 'untrusted':
    expected_sigtype = 'normal'

  # If an uncaught signal occurs under QEMU (on ARM), the exit status
  # contains the signal number, mangled as per IndirectSignal().  We
  # extract the unadulterated signal number from QEMU's log message in
  # stderr instead.  If we are not using QEMU, or no signal is raised
  # under QEMU, this is a no-op.
  if stderr is not None:
    exit_status = GetQemuSignalFromStderr(stderr, exit_status)

  exitOk = exit_status in expected_statuses
  if stderr is None:
    printed_sigtype = 'unknown'
  else:
    printed_signum, printed_sigtype = GetNaClSignalInfoFromStderr(stderr)
    exitOk = exitOk and printed_sigtype == expected_sigtype

  if exitOk and not failed:
    return True
  if failed:
    Print('Command failed')
  else:
    Print('\nERROR: Command returned %s with type=%r, '
          'but expected %s with type=%r for platform=%s' %
          (exit_status, printed_sigtype,
           expected_statuses, expected_sigtype,
           GlobalPlatform))
    if stderr is not None:
      Banner('Stdout')
      Print(stdout)
      Banner('Stderr')
      Print(stderr)
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

def ProcessLogOutput(stdout, stderr):
  output_processor = GlobalSettings['process_output']
  if output_processor:
    output_processor_cmd = DestringifyList(output_processor)
    # Also, get the output from logout (to get NaClLog output in Windows).
    log_output = open(GlobalSettings['log_file']).read()
    # Assume the log processor does not care about the order of the lines.
    all_output = log_output + stdout + stderr
    if not test_lib.RunCmdWithInput(output_processor_cmd, all_output):
      Print(FailureMessage())
      return False
  return True

def main(argv):
  global GlobalPlatform
  global GlobalReportStream
  command = ProcessOptions(argv)

  if GlobalSettings['report']:
    GlobalReportStream.append(open(GlobalSettings['report'], 'w'))

  if not GlobalSettings['name']:
    GlobalSettings['name'] = command[0]

  if GlobalSettings['osenv']:
    Banner('setting environment')
    env_vars = DestringifyList(GlobalSettings['osenv'])
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
            and not GlobalSettings['process_output']
            )
    # If python ever changes popen.stdout.read() to not risk deadlock,
    # we could stream and capture, and use RunTestWithInputOutput instead.
    (total_time, exit_status, failed) = test_lib.RunTestWithInput(command,
                                                                  stdin_data)
    PrintTotalTime(total_time)
    if not CheckExitStatus(failed, GlobalSettings['exit_status'], exit_status,
                           None, None):
      return -1
  else:
    (total_time, exit_status,
     failed, stdout, stderr) = test_lib.RunTestWithInputOutput(
         command, stdin_data)
    PrintTotalTime(total_time)
    if not CheckExitStatus(failed, GlobalSettings['exit_status'], exit_status,
                           stdout, stderr):
      return -1
    if not CheckGoldenOutput(stdout, stderr):
      return -1
    if not ProcessLogOutput(stdout, stderr):
      return -1

  if not CheckTimeBounds(total_time):
    return -1

  Print(SuccessMessage())
  return 0


if __name__ == '__main__':
  sys.exit(main(sys.argv[1:]))
