# Copyright 2018 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import subprocess
import unittest

import mock

from core import cli_helpers


class CLIHelpersTest(unittest.TestCase):
  def testUnsupportedColor(self):
    with self.assertRaises(AssertionError):
      cli_helpers.colored('message', 'pink')

  @mock.patch('__builtin__.print')
  def testPrintsInfo(self, print_mock):
    cli_helpers.info('foo {sval} {ival}', sval='s', ival=42)
    print_mock.assert_called_once_with('foo s 42')

  @mock.patch('__builtin__.print')
  def testPrintsComment(self, print_mock):
    cli_helpers.comment('foo')
    print_mock.assert_called_once_with('\033[93mfoo\033[0m')

  @mock.patch('__builtin__.print')
  @mock.patch('sys.exit')
  def testFatal(self, sys_exit_mock, print_mock):
    cli_helpers.fatal('foo')
    print_mock.assert_called_once_with('\033[91mfoo\033[0m')
    sys_exit_mock.assert_called_once()

  @mock.patch('__builtin__.print')
  def testPrintsError(self, print_mock):
    cli_helpers.error('foo')
    print_mock.assert_called_once_with('\033[91mfoo\033[0m')

  @mock.patch('__builtin__.print')
  def testPrintsStep(self, print_mock):
    long_step_name = 'foobar' * 15
    cli_helpers.step(long_step_name)
    self.assertListEqual(print_mock.call_args_list, [
        mock.call('\033[92m' + ('=' * 90) + '\033[0m'),
        mock.call('\033[92m' + long_step_name + '\033[0m'),
        mock.call('\033[92m' + ('=' * 90) + '\033[0m'),
    ])

  @mock.patch('__builtin__.print')
  @mock.patch('__builtin__.raw_input')
  def testAskAgainOnInvalidAnswer(self, raw_input_mock, print_mock):
    raw_input_mock.side_effect = ['foobar', 'y']
    self.assertTrue(cli_helpers.ask('Ready?'))
    self.assertListEqual(print_mock.mock_calls, [
      mock.call('\033[96mReady? [no/YES] \033[0m', end=' '),
      mock.call('\033[91mPlease respond with "no" or "yes".\033[0m'),
      mock.call('\033[96mReady? [no/YES] \033[0m', end=' ')
    ])

  @mock.patch('__builtin__.print')
  @mock.patch('__builtin__.raw_input')
  def testAskWithCustomAnswersAndDefault(self, raw_input_mock, print_mock):
    raw_input_mock.side_effect = ['']
    self.assertFalse(
        cli_helpers.ask('Ready?', {'foo': True, 'bar': False}, default='bar'))
    print_mock.assert_called_once_with(
        '\033[96mReady? [BAR/foo] \033[0m', end=' ')

  @mock.patch('__builtin__.print')
  @mock.patch('__builtin__.raw_input')
  def testAskNoDefaultCustomAnswersAsList(self, raw_input_mock, print_mock):
    raw_input_mock.side_effect = ['', 'FoO']
    self.assertEqual(cli_helpers.ask('Ready?', ['foo', 'bar']), 'foo')
    self.assertListEqual(print_mock.mock_calls, [
      mock.call('\033[96mReady? [foo/bar] \033[0m', end=' '),
      mock.call('\033[91mPlease respond with "bar" or "foo".\033[0m'),
      mock.call('\033[96mReady? [foo/bar] \033[0m', end=' ')
    ])

  def testAskWithInvalidDefaultAnswer(self):
    with self.assertRaises(ValueError):
      cli_helpers.ask('Ready?', ['foo', 'bar'], 'baz')

  @mock.patch('__builtin__.print')
  @mock.patch('subprocess.check_call')
  @mock.patch('__builtin__.open')
  @mock.patch('datetime.datetime')
  def testCheckLog(
      self, dt_mock, open_mock, check_call_mock, print_mock):
    file_mock = mock.Mock()
    open_mock.return_value.__enter__.return_value = file_mock
    dt_mock.now.return_value.strftime.return_value = '_2018_12_10_16_22_11'

    cli_helpers.check_log(
        ['command', 'arg1'], '/tmp/tmpXYZ.tmp', env={'foo': 'bar'})

    check_call_mock.assert_called_once_with(
        ['command', 'arg1'], stdout=file_mock, stderr=subprocess.STDOUT,
        shell=False, env={'foo': 'bar'})
    open_mock.assert_called_once_with('/tmp/tmpXYZ.tmp', 'w')
    self.assertListEqual(print_mock.mock_calls, [
      mock.call('\033[94mcommand arg1\033[0m'),
      mock.call('\033[94mLogging stdout & stderr to /tmp/tmpXYZ.tmp\033[0m'),
    ])

  @mock.patch('__builtin__.print')
  @mock.patch('core.cli_helpers.error')
  @mock.patch('subprocess.check_call')
  @mock.patch('subprocess.call')
  @mock.patch('__builtin__.open')
  def testCheckLogError(
      self, open_mock, call_mock, check_call_mock, error_mock, print_mock):
    del print_mock, open_mock  # Unused.
    check_call_mock.side_effect = [subprocess.CalledProcessError(87, ['cmd'])]

    with self.assertRaises(subprocess.CalledProcessError):
      cli_helpers.check_log(['cmd'], '/tmp/tmpXYZ.tmp')

    call_mock.assert_called_once_with(['cat', '/tmp/tmpXYZ.tmp'])
    self.assertListEqual(error_mock.mock_calls, [
      mock.call('=' * 80),
      mock.call('Received non-zero return code. Log content:'),
      mock.call('=' * 80),
      mock.call('=' * 80),
    ])

  @mock.patch('__builtin__.print')
  @mock.patch('subprocess.check_call')
  def testRun(self, check_call_mock, print_mock):
    check_call_mock.side_effect = [subprocess.CalledProcessError(87, ['cmd'])]
    with self.assertRaises(subprocess.CalledProcessError):
      cli_helpers.run(['cmd', 'arg with space'], env={'a': 'b'})
    check_call_mock.assert_called_once_with(
        ['cmd', 'arg with space'], env={'a': 'b'})
    print_mock.assert_called_once_with('\033[94mcmd \'arg with space\'\033[0m')

  @mock.patch('__builtin__.print')
  @mock.patch('subprocess.check_call')
  def testRunOkFail(self, check_call_mock, print_mock):
    del print_mock  # Unused.
    check_call_mock.side_effect = [subprocess.CalledProcessError(87, ['cmd'])]
    cli_helpers.run(['cmd'], ok_fail=True)

  def testRunWithNonListCommand(self):
    with self.assertRaises(ValueError):
      cli_helpers.run('cmd with args')


if __name__ == "__main__":
  unittest.main()
