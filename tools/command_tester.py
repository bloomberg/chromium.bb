#!/usr/bin/python
# Copyright 2008, Google Inc.
# All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are
# met:
#
#     * Redistributions of source code must retain the above copyright
# notice, this list of conditions and the following disclaimer.
#     * Redistributions in binary form must reproduce the above
# copyright notice, this list of conditions and the following disclaimer
# in the documentation and/or other materials provided with the
# distribution.
#     * Neither the name of Google Inc. nor the names of its
# contributors may be used to endorse or promote products derived from
# this software without specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
# "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
# LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
# A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
# OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
# SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
# LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
# DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
# THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
# OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.


"""Simple testing harness for running commands and checking expected output.

This harness is used instead of shell scripts to ensure windows compatibility

"""


# python imports
import getopt
import os
import sys
import time

# local imports
import test_lib

GlobalReportStream = [sys.stdout]

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

  diff = list(test_lib.DiffStringsIgnoringWhiteSpace(actual, golden))
  diff = '\n'.join(diff)
  if diff:
    Banner('Error %s diff found' % output_type)
    Print(diff)
    Banner('Potential New Golden Output')
    Print(actual)
    Print(fail_msg)
    return True
  return False


GlobalSettings = {
    'exit_status': 0,
    'osenv': '',

    'name': None,
    'report': None,

    'stdin': None,
    'logout': None,

    'stdout_golden': None,
    'stderr_golden': None,
    'log_golden': None,

    'stdout_filter': None,
    'stderr_filter': None,
    'log_filter': None,

    'filter_validator': 0,

    'time_warning': 0,
    'time_error': 0,

    'run_under': None,
}


# NOTE: The failure/success messages are parsed by Pulse and should
# match the regular expression used by "golden-test" post-processor.
# See pulse.xml for details.
def FailureMessage():
  return 'RESULT: %s FAILED' % GlobalSettings['name']


def SuccessMessage():
  return 'RESULT: %s PASSED' % GlobalSettings['name']


def MassageExitStatus(v):
  status_map = {
      'linux2': -11,
      'darwin': -10,
      'cygwin': -1073741819,  # 0x3ffffffb
      'win32':  -1073741819,  # 0x3ffffffb
      }
  if v == 'segfault':
    assert sys.platform in status_map
    return status_map[sys.platform]
  else:
    return int(v)


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
    if option == 'exit_status':
      GlobalSettings[option] = MassageExitStatus(a)
    elif type(GlobalSettings[option]) == int:
      GlobalSettings[option] = int(a)
    else:
      GlobalSettings[option] = a
  # return the unprocessed options, i.e. the command
  return args


# on linux return codes are 8 bit. So -n = 256 + (-n) (eg: 243 = -11)
# python does not understand this 8 bit wraparound.
# Thus, we convert the desired val and returned val if negative to postive
# before comparing.
def ExitStatusIsOK(expected, actual):
  if sys.platform == 'linux2':  # convert only for linux
    expected = (expected + 256) % 256
    actual = (actual + 256) % 256
  return expected == actual


def main(argv):
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
    env = GlobalSettings['osenv'].split(',')
    for e in env:
      # = is valid in val of an env
      eq_pos=e.find('=')
      key = e[:eq_pos]
      val = e[eq_pos+1:]
      Print('[%s] = [%s]' % (key, val))
      os.putenv(key, val)

  if GlobalSettings['logout']:
    try:
      os.unlink(GlobalSettings['logout'])  # might not pre-exist
    except OSError:
      pass

  stdin_data = ''
  if GlobalSettings['stdin']:
    stdin_data = open(GlobalSettings['stdin'])

  run_under = GlobalSettings['run_under']
  if run_under:
    command = run_under.split(',') + command

  start_time = time.time()
  Banner('running %s' % str(command))
  # print the command in copy-and-pastable fashion
  print " ".join(command)
  _, exit_status, failed, stdout, stderr = test_lib.RunTestWithInputOutput(
      command, stdin_data)
  total_time = time.time() - start_time

  if not ExitStatusIsOK(GlobalSettings['exit_status'], exit_status) or failed:
    if failed:
      Print('command failed')
    else:
      Print('command returned unexpected exit status %d' % exit_status)
    Banner('Stdout')
    Print(stdout)
    Banner('Stderr')
    Print(stderr)
    Print(FailureMessage())
    return -1

  if GlobalSettings['filter_validator']:
    stdout = test_lib.RegexpFilterLines(r'^(?!VALIDATOR)', stdout)

  for (stream, getter) in [
      ('stdout', lambda: stdout),
      ('stderr', lambda: stderr),
      ('log', lambda: open(GlobalSettings['logout']).read()),
      ]:
    golden = stream + '_golden'
    if GlobalSettings[golden]:
      golden_data = open(GlobalSettings[golden]).read()
      filt = stream + '_filter'
      actual = getter()
      if GlobalSettings[filt]:
        actual = test_lib.RegexpFilterLines(GlobalSettings[filt], actual)
      if DifferentFromGolden(actual, golden_data, stream, FailureMessage()):
        return -1

  Print('Test %s took %f secs' % (GlobalSettings['name'], total_time))
  if GlobalSettings['time_error']:
    if total_time > GlobalSettings['time_error']:
      Print('ERROR: should have taken less than %f secs' %
            (GlobalSettings['time_error']))
      Print(FailureMessage())
      return -1

  if GlobalSettings['time_warning']:
    if total_time > GlobalSettings['time_warning']:
      Print('WARNING: should have taken less than %f secs' %
            (GlobalSettings['time_warning']))


  Print(SuccessMessage())
  return 0


if __name__ == '__main__':
  sys.exit(main(sys.argv[1:]))
