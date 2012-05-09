#!/usr/bin/python
# Copyright (c) 2012 The Native Client Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.


"""Simple testing harness for running commands and checking expected output.

This harness is used instead of shell scripts to ensure windows compatibility

"""


# python imports
import getopt
import os
import re
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


def DifferentFromGolden(actual, golden, output_type):
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
    return True
  return False


def ResetGlobalSettings():
  global GlobalSettings
  GlobalSettings = {
      'exit_status': 0,
      'using_nacl_signal_handler': False,

      # When declares_exit_status is set, we read the expected exit
      # status from stderr.  We look for a line of the form
      # "** intended_exit_status=X".  This allows crash tests to
      # declare their expected exit status close to where the crash is
      # generated, rather than in a Scons file.  It reduces the risk
      # that the test passes accidentally by crashing during setup.
      'declares_exit_status': False,

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

      # Number of times a test is run.
      # This is useful for getting multiple samples for time perf tests.
      'num_runs': 1,

      # Scripts for processing output along with its arguments.
      # This script is given the output of a single run.
      'process_output_single': None,
      # This script is given the concatenated output of all |num_runs|, after
      # having been filtered by |process_output_single| for individual runs.
      'process_output_combined': None,

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

# The following messages match gtest's formatting.  This improves log
# greppability for people who primarily work on Chrome.  It also allows
# gtest-specific hooks on the buildbots to fire.
# The buildbots expect test names in the format "suite_name.test_name", so we
# prefix the test name with a bogus suite name (nacl).
def RunMessage():
  return '[ RUN      ] %s' % (GlobalSettings['name'],)

def FailureMessage(total_time):
  return '[  FAILED  ] %s (%d ms)' % (GlobalSettings['name'],
                                      total_time * 1000.0)

def SuccessMessage(total_time):
  return '[       OK ] %s (%d ms)' % (GlobalSettings['name'],
                                      total_time * 1000.0)

def LogPerfResult(graph_name, trace_name, value, units):
  # NOTE: This RESULT message is parsed by Chrome's perf graph generator.
  Print('RESULT %s: %s= %s %s' %
        (graph_name, trace_name, value, units))

def PrintTotalTime(total_time):
  if int(GlobalSettings['track_cmdtime']):
    LogPerfResult(GlobalSettings['name'],
                  'TOTAL_' + GlobalSettings['perf_env_description'],
                  '%f' % total_time,
                  'secs')


# On POSIX systems, exit() codes are 8-bit.  You cannot use exit() to
# make it look like the process was killed by a signal.  Instead,
# NaCl's signal handler encodes the signal number into the exit() code
# by returning with exit(-signum) or equivalently, exit((-signum) & 0xff).
# NaCl uses the same encoding on Windows.
def IndirectSignal(signum):
  return (-signum) & 0xff

# Windows exit codes that indicate unhandled exceptions.
STATUS_ACCESS_VIOLATION = 0xc0000005
STATUS_PRIVILEGED_INSTRUCTION = 0xc0000096

# Python's wrapper for GetExitCodeProcess() treats the STATUS_* values
# as negative, although the unsigned values are used in headers and
# are more widely recognised.
def MungeWindowsErrorExit(num):
  return num - 0x100000000

# If a crash occurs in x86-32 untrusted code on Windows, the kernel
# apparently gets confused about the cause.  It fails to take %cs into
# account when examining the faulting instruction, so it looks at the
# wrong instruction, so we could get either of the errors below.
# See http://code.google.com/p/nativeclient/issues/detail?id=1689
win32_untrusted_crash_exit = [
    MungeWindowsErrorExit(STATUS_ACCESS_VIOLATION),
    MungeWindowsErrorExit(STATUS_PRIVILEGED_INSTRUCTION)]

# We patch Windows' KiUserExceptionDispatcher on x86-64 to terminate
# the process safely when untrusted code crashes.  We get the exit
# code associated with the HLT instruction.
win64_exit_via_ntdll_patch = [
    MungeWindowsErrorExit(STATUS_PRIVILEGED_INSTRUCTION)]


# Mac OS X returns SIGBUS in most of the cases where Linux returns
# SIGSEGV, except for actual x86 segmentation violations.
status_map = {
    'sigabrt' : {
        'linux2': [-6], # SIGABRT
        'darwin': [-6], # SIGABRT
        # On Windows, NaClAbort() exits using the HLT instruction.
        'win32': [MungeWindowsErrorExit(STATUS_PRIVILEGED_INSTRUCTION)],
        'win64': [MungeWindowsErrorExit(STATUS_PRIVILEGED_INSTRUCTION)],
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
        'linux2': [-11], # SIGSEGV
        'darwin': [-11], # SIGSEGV
        'win32':  win32_untrusted_crash_exit,
        'win64':  win64_exit_via_ntdll_patch,
        },
    'untrusted_segfault': {
        'linux2': [-11], # SIGSEGV
        'darwin': [-10], # SIGBUS
        'win32':  win32_untrusted_crash_exit,
        'win64':  win64_exit_via_ntdll_patch,
        'win_fakesig': 11, # SIGSEGV
        },
    'untrusted_sigsegv_or_equivalent': {
        'linux2': [-11], # SIGSEGV
        'darwin': [-11], # SIGSEGV
        'win32':  win32_untrusted_crash_exit,
        'win64':  win64_exit_via_ntdll_patch,
        },
    'trusted_segfault': {
        'linux2': [-11], # SIGSEGV
        'darwin': [-10], # SIGBUS
        'win32':  [MungeWindowsErrorExit(STATUS_ACCESS_VIOLATION)],
        'win64':  [MungeWindowsErrorExit(STATUS_ACCESS_VIOLATION)],
        'win_fakesig': 11, # SIGSEGV
        },
    'trusted_sigsegv_or_equivalent': {
        'linux2': [-11], # SIGSEGV
        'darwin': [-11], # SIGSEGV
        'win32':  [],
        'win64':  [],
        'win_fakesig': 11, # SIGSEGV
        },
    # This is like 'untrusted_segfault', but without the 'untrusted_'
    # prefix which marks the status type as expecting a
    # gracefully-printed exit message from nacl_signal_common.c.  This
    # is a special case because we use different methods for writing
    # the exception stack frame on different platforms.  On Mac and
    # Windows, NaCl uses a system call which will detect unwritable
    # pages, so the exit status appears as an unhandled fault from
    # untrusted code.  On Linux, NaCl's signal handler writes the
    # frame directly, so the exit status comes from getting a SIGSEGV
    # inside the SIGSEGV handler.
    'unwritable_exception_stack': {
        'linux2': [-11], # SIGSEGV
        'darwin': [-10], # SIGBUS
        'win32':  win32_untrusted_crash_exit,
        'win64':  win64_exit_via_ntdll_patch,
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
    sys.exit(1)

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
  sigNum = None
  sigType = 'normal'

  lines = stderr.splitlines()

  # Scan for signal msg in reverse order
  for curline in reversed(lines):
    words = curline.split()
    if len(words) > 4 and words[0] == '**' and words[1] == 'Signal':
      sigNum = int(words[2])
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

def FormatExitStatus(number):
  # Include the hex version because it makes the Windows error exit
  # statuses (STATUS_*) more recognisable.
  return '%i (0x%x)' % (number, number & 0xffffffff)

def PrintStdStreams(stdout, stderr):
  if stderr is not None:
    Banner('Stdout for %s:' % os.path.basename(GlobalSettings['name']))
    Print(stdout)
    Banner('Stderr for %s:' % os.path.basename(GlobalSettings['name']))
    Print(stderr)

def GetIntendedExitStatuses(stderr):
  statuses = []
  for line in stderr.splitlines():
    match = re.match(r'\*\* intended_exit_status=(.*)$', line)
    if match is not None:
      statuses.append(match.group(1))
  return statuses

def CheckExitStatus(failed, req_status, using_nacl_signal_handler,
                    exit_status, stdout, stderr):
  if GlobalSettings['declares_exit_status']:
    assert req_status == 0
    intended_statuses = GetIntendedExitStatuses(stderr)
    if len(intended_statuses) == 0:
      Print('\nERROR: Command returned exit status %s but did not output an '
            'intended_exit_status line to stderr - did it exit too early?'
            % FormatExitStatus(exit_status))
      return False
    elif len(intended_statuses) != 1:
      Print('\nERROR: Command returned exit status %s but produced '
            'multiple intended_exit_status lines (%s)'
            % (FormatExitStatus(exit_status), ', '.join(intended_statuses)))
      return False
    else:
      req_status = intended_statuses[0]

  expected_sigtype = 'normal'
  if req_status in status_map:
    expected_statuses = status_map[req_status][GlobalPlatform]
    if using_nacl_signal_handler:
      if req_status.startswith('trusted_'):
        expected_sigtype = 'trusted'
      elif req_status.startswith('untrusted_'):
        expected_sigtype = 'untrusted'
  else:
    expected_statuses = [int(req_status)]

  # On 32-bit Windows, we cannot catch a signal that occurs in x86-32
  # untrusted code, so it always appears as a 'normal' exit, which
  # means that the signal handler does not print a message.
  if GlobalPlatform == 'win32' and expected_sigtype == 'untrusted':
    expected_sigtype = 'normal'

  if expected_sigtype == 'normal':
    expected_printed_signum = None
  else:
    if sys.platform == 'win32':
      # On Windows, the NaCl signal handler maps the fault type to a
      # Unix-like signal number.
      expected_printed_signum = status_map[req_status]['win_fakesig']
    else:
      # On Unix, the NaCl signal handler reports the actual signal number.
      assert len(expected_statuses) == 1
      assert expected_statuses[0] < 0
      expected_printed_signum = -expected_statuses[0]
    expected_statuses = [IndirectSignal(expected_printed_signum)]

  # If an uncaught signal occurs under QEMU (on ARM), the exit status
  # contains the signal number, mangled as per IndirectSignal().  We
  # extract the unadulterated signal number from QEMU's log message in
  # stderr instead.  If we are not using QEMU, or no signal is raised
  # under QEMU, this is a no-op.
  if stderr is not None:
    exit_status = GetQemuSignalFromStderr(stderr, exit_status)

  msg = '\nERROR: Command returned exit status %s but we expected %s' % (
      FormatExitStatus(exit_status),
      ' or '.join(FormatExitStatus(value) for value in expected_statuses))
  if exit_status not in expected_statuses:
    Print(msg)
    failed = True
  if stderr is not None:
    expected_printed = (expected_printed_signum, expected_sigtype)
    actual_printed = GetNaClSignalInfoFromStderr(stderr)
    msg = ('\nERROR: Command printed the signal info %s to stderr '
           'but we expected %s' %
           (actual_printed, expected_printed))
    if actual_printed != expected_printed:
      Print(msg)
      failed = True

  return not failed

def CheckTimeBounds(total_time):
  if GlobalSettings['time_error']:
    if total_time > GlobalSettings['time_error']:
      Print('ERROR: should have taken less than %f secs' %
            (GlobalSettings['time_error']))
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
      if DifferentFromGolden(actual, golden_data, stream):
        return False
  return True

def ProcessLogOutputSingle(stdout, stderr):
  output_processor = GlobalSettings['process_output_single']
  if output_processor is None:
    return (True, stdout, stderr)
  else:
    output_processor_cmd = DestringifyList(output_processor)
    # Also, get the output from log_file to get NaClLog output in Windows.
    log_output = open(GlobalSettings['log_file']).read()
    # Assume the log processor does not care about the order of the lines.
    all_output = log_output + stdout + stderr
    _, retcode, failed, new_stdout, new_stderr = \
        test_lib.RunTestWithInputOutput(output_processor_cmd, all_output)
    # Print the result, since we have done some processing and we need
    # to have the processed data. However, if we intend to process it some
    # more later via process_output_combined, do not duplicate the data here.
    # Only print out the final result!
    if not GlobalSettings['process_output_combined']:
      PrintStdStreams(new_stdout, new_stderr)
    if retcode != 0 or failed:
      return (False, new_stdout, new_stderr)
    else:
      return (True, new_stdout, new_stderr)

def ProcessLogOutputCombined(stdout, stderr):
  output_processor = GlobalSettings['process_output_combined']
  if output_processor is None:
    return True
  else:
    output_processor_cmd = DestringifyList(output_processor)
    all_output = stdout + stderr
    _, retcode, failed, new_stdout, new_stderr = \
        test_lib.RunTestWithInputOutput(output_processor_cmd, all_output)
    # Print the result, since we have done some processing.
    PrintStdStreams(new_stdout, new_stderr)
    if retcode != 0 or failed:
      return False
    else:
      return True

def DoRun(command, stdin_data):
  """
  Run the command, given stdin_data. Returns a return code (0 is good)
  and optionally a captured version of stdout, stderr from the run
  (if the global setting capture_output is true).
  """
  # Initialize stdout, stderr to indicate we have not captured
  # any of stdout or stderr.
  stdout = ''
  stderr = ''
  if not int(GlobalSettings['capture_output']):
    # We are only blurting out the stdout and stderr, not capturing it
    # for comparison, etc.
    assert (not GlobalSettings['stdout_golden']
            and not GlobalSettings['stderr_golden']
            and not GlobalSettings['log_golden']
            and not GlobalSettings['filter_regex']
            and not GlobalSettings['filter_inverse']
            and not GlobalSettings['filter_group_only']
            and not GlobalSettings['process_output_single']
            and not GlobalSettings['process_output_combined']
            )
    # If python ever changes popen.stdout.read() to not risk deadlock,
    # we could stream and capture, and use RunTestWithInputOutput instead.
    (total_time, exit_status, failed) = test_lib.RunTestWithInput(command,
                                                                  stdin_data)
    PrintTotalTime(total_time)
    if not CheckExitStatus(failed,
                           GlobalSettings['exit_status'],
                           GlobalSettings['using_nacl_signal_handler'],
                           exit_status, None, None):
      Print(FailureMessage(total_time))
      return (1, stdout, stderr)
  else:
    (total_time, exit_status,
     failed, stdout, stderr) = test_lib.RunTestWithInputOutput(
         command, stdin_data)
    PrintTotalTime(total_time)
    # CheckExitStatus may spew stdout/stderr when there is an error.
    # Otherwise, we do not spew stdout/stderr in this case (capture_output).
    if not CheckExitStatus(failed,
                           GlobalSettings['exit_status'],
                           GlobalSettings['using_nacl_signal_handler'],
                           exit_status, stdout, stderr):
      PrintStdStreams(stdout, stderr)
      Print(FailureMessage(total_time))
      return (1, stdout, stderr)
    if not CheckGoldenOutput(stdout, stderr):
      Print(FailureMessage(total_time))
      return (1, stdout, stderr)
    success, stdout, stderr = ProcessLogOutputSingle(stdout, stderr)
    if not success:
      Print(FailureMessage(total_time) + ' ProcessLogOutputSingle failed!')
      return (1, stdout, stderr)

  if not CheckTimeBounds(total_time):
    Print(FailureMessage(total_time))
    return (1, stdout, stderr)

  Print(SuccessMessage(total_time))
  return (0, stdout, stderr)


def Main(argv):
  command = ProcessOptions(argv)

  if GlobalSettings['report']:
    GlobalReportStream.append(open(GlobalSettings['report'], 'w'))

  if not GlobalSettings['name']:
    GlobalSettings['name'] = command[0]
  GlobalSettings['name'] = os.path.basename(GlobalSettings['name'])

  Print(RunMessage())
  num_runs = GlobalSettings['num_runs']
  if num_runs > 1:
    Print(' (running %d times)' % num_runs)

  if GlobalSettings['osenv']:
    Banner('setting environment')
    env_vars = DestringifyList(GlobalSettings['osenv'])
  else:
    env_vars = []
  for env_var in env_vars:
    key, val = env_var.split('=', 1)
    Print('[%s] = [%s]' % (key, val))
    os.environ[key] = val

  stdin_data = ''
  if GlobalSettings['stdin']:
    stdin_data = open(GlobalSettings['stdin'])

  run_under = GlobalSettings['run_under']
  if run_under:
    command = run_under.split(',') + command

  Banner('running %s' % str(command))
  # print the command in copy-and-pastable fashion
  print ' '.join(env_vars + command)

  # Concatenate output when running multiple times (e.g., for timing).
  combined_stdout = ''
  combined_stderr = ''
  cur_runs = 0
  num_runs = GlobalSettings['num_runs']
  while cur_runs < num_runs:
    cur_runs += 1
    # Clear out previous log_file.
    if GlobalSettings['log_file']:
      try:
        os.unlink(GlobalSettings['log_file'])  # might not pre-exist
      except OSError:
        pass
    ret_code, stdout, stderr = DoRun(command, stdin_data)
    if ret_code != 0:
      return ret_code
    combined_stdout += stdout
    combined_stderr += stderr
  # Process the log output after all the runs.
  success = ProcessLogOutputCombined(combined_stdout, combined_stderr)
  if not success:
    # Bogus time, since only ProcessLogOutputCombined failed.
    Print(FailureMessage(0.0) + ' ProcessLogOutputCombined failed!')
    return 1
  return 0

if __name__ == '__main__':
  retval = Main(sys.argv[1:])
  # Add some whitepsace to make the logs easier to read.
  sys.stdout.write('\n\n')
  sys.exit(retval)
