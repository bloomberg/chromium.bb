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
    'osenv': None,

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

    'time_warning': 0,
    'time_error': 0,
}


# NOTE: The failure/success messages are parsed by Pulse and should
# match the regular expression used by "golden-test" post-processor.
# See pulse.xml for details.
def FailureMessage():
  return 'RESULT: %s FAILED' % GlobalSettings['name']


def SuccessMessage():
  return 'RESULT: %s PASSED' % GlobalSettings['name']


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
  # return the unprocessed options, i.e. the command
  return args


def main(argv):
  global GlobalReportStream
  command = ProcessOptions(argv)

  if GlobalSettings['report']:
    GlobalReportStream.append(open(GlobalSettings['report'], 'w'))

  if not GlobalSettings['name']:
    GlobalSettings['name'] = command[0]

  if GlobalSettings['osenv']:
    env = GlobalSettings['osenv'].split(',')
    for e in env:
      key, val = e.split('=')
      os.putenv(key, val)

  if GlobalSettings['logout']:
    try:
      os.unlink(GlobalSettings['logout'])  # might not pre-exist
    except OSError:
      pass

  stdin_data = ''
  if GlobalSettings['stdin']:
    stdin_data = open(GlobalSettings['stdin']).read()

  start_time = time.time()
  Banner('running %s' % str(command))
  _, exit_status, failed, stdout, stderr = test_lib.RunTestWithInputOutput(
      command, stdin_data)
  total_time = time.time() - start_time

  if exit_status != GlobalSettings['exit_status'] or failed:
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

  if GlobalSettings['stdout_golden']:
    stdout_golden = open(GlobalSettings['stdout_golden']).read()
    if GlobalSettings['stdout_filter']:
      stdout = test_lib.RegexpFilterLines(GlobalSettings['stdout_filter'],
                                          stdout)
    if DifferentFromGolden(stdout, stdout_golden, 'Stdout', FailureMessage()):
      return -1

  if GlobalSettings['stderr_golden']:
    stderr_golden = open(GlobalSettings['stderr_golden']).read()
    if GlobalSettings['stderr_filter']:
      stderr = test_lib.RegexpFilterLines(GlobalSettings['stderr_filter'],
                                          stderr)
    if DifferentFromGolden(stderr, stderr_golden, 'Stderr', FailureMessage()):
      return -1

  if GlobalSettings['log_golden']:
    log_golden = open(GlobalSettings['log_golden']).read()
    if GlobalSettings['log_filter']:
      log =  open(GlobalSettings['logout']).read()
      log = test_lib.RegexpFilterLines(GlobalSettings['log_filter'],
                                       log)
    if DifferentFromGolden(log, log_golden, 'Log', FailureMessage()):
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
