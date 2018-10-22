#!/usr/bin/env python
# Copyright (c) 2018 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import json
import os
import sys
import unittest

ROOT_DIR = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, ROOT_DIR)

import metrics
import cStringIO

from third_party import mock


class TimeMock(object):
  def __init__(self):
    self._count = 0

  def __call__(self):
    self._count += 1
    return self._count * 1000


class MetricsCollectorTest(unittest.TestCase):
  def setUp(self):
    self.config_file = os.path.join(ROOT_DIR, 'metrics.cfg')
    self.collector = metrics.MetricsCollector()

    # Keep track of the URL requests, file reads/writes and subprocess spawned.
    self.urllib2 = mock.Mock()
    self.print_notice = mock.Mock()
    self.print_version_change = mock.Mock()
    self.Popen = mock.Mock()
    self.FileWrite = mock.Mock()
    self.FileRead = mock.Mock()

    mock.patch('metrics.urllib2', self.urllib2).start()
    mock.patch('metrics.subprocess.Popen', self.Popen).start()
    mock.patch('metrics.gclient_utils.FileWrite', self.FileWrite).start()
    mock.patch('metrics.gclient_utils.FileRead', self.FileRead).start()
    mock.patch('metrics.metrics_utils.print_notice', self.print_notice).start()
    mock.patch(
        'metrics.metrics_utils.print_version_change',
        self.print_version_change).start()

    # Patch the methods used to get the system information, so we have a known
    # environment.
    mock.patch('metrics.tempfile.mkstemp',
               lambda: (None, '/tmp/metrics.json')).start()
    mock.patch('metrics.time.time',
               TimeMock()).start()
    mock.patch('metrics.metrics_utils.get_python_version',
               lambda: '2.7.13').start()
    mock.patch('metrics.gclient_utils.GetMacWinOrLinux',
               lambda: 'linux').start()
    mock.patch('metrics.detect_host_arch.HostArch',
               lambda: 'x86').start()
    mock.patch('metrics_utils.get_repo_timestamp',
               lambda _: 1234).start()

    self.default_metrics = {
        "python_version": "2.7.13",
        "execution_time": 1000,
        "timestamp": 0,
        "exit_code": 0,
        "command": "fun",
        "depot_tools_age": 1234,
        "host_arch": "x86",
        "host_os": "linux",
    }

    self.addCleanup(mock.patch.stopall)

  def assert_writes_file(self, expected_filename, expected_content):
    self.assertEqual(len(self.FileWrite.mock_calls), 1)
    filename, content = self.FileWrite.mock_calls[0][1]

    self.assertEqual(filename, expected_filename)
    self.assertEqual(json.loads(content), expected_content)

  def test_writes_config_if_not_exists(self):
    self.FileRead.side_effect = [IOError(2, "No such file or directory")]
    mock_response = mock.Mock()
    self.urllib2.urlopen.side_effect = [mock_response]
    mock_response.getcode.side_effect = [200]

    self.assertTrue(self.collector.config.is_googler)
    self.assertIsNone(self.collector.config.opted_in)
    self.assertEqual(self.collector.config.countdown, 10)

    self.assert_writes_file(
        self.config_file,
        {'is-googler': True, 'countdown': 10, 'opt-in': None, 'version': 0})

  def test_writes_config_if_not_exists_non_googler(self):
    self.FileRead.side_effect = [IOError(2, "No such file or directory")]
    mock_response = mock.Mock()
    self.urllib2.urlopen.side_effect = [mock_response]
    mock_response.getcode.side_effect = [403]

    self.assertFalse(self.collector.config.is_googler)
    self.assertIsNone(self.collector.config.opted_in)
    self.assertEqual(self.collector.config.countdown, 10)

    self.assert_writes_file(
        self.config_file,
        {'is-googler': False, 'countdown': 10, 'opt-in': None, 'version': 0})

  def test_disables_metrics_if_cant_write_config(self):
    self.FileRead.side_effect = [IOError(2, 'No such file or directory')]
    mock_response = mock.Mock()
    self.urllib2.urlopen.side_effect = [mock_response]
    mock_response.getcode.side_effect = [200]
    self.FileWrite.side_effect = [IOError(13, 'Permission denied.')]

    self.assertTrue(self.collector.config.is_googler)
    self.assertFalse(self.collector.config.opted_in)
    self.assertEqual(self.collector.config.countdown, 10)

  def assert_collects_metrics(self, update_metrics=None):
    expected_metrics = self.default_metrics
    self.default_metrics.update(update_metrics or {})
    # Assert we invoked the script to upload them.
    self.Popen.assert_called_with(
        [sys.executable, metrics.UPLOAD_SCRIPT], stdin=metrics.subprocess.PIPE)
    # Assert we collected the right metrics.
    write_call = self.Popen.return_value.stdin.write.call_args
    collected_metrics = json.loads(write_call[0][0])
    self.assertTrue(self.collector.collecting_metrics)
    self.assertEqual(collected_metrics, expected_metrics)

  def test_collects_system_information(self):
    """Tests that we collect information about the runtime environment."""
    self.FileRead.side_effect = [
        '{"is-googler": true, "countdown": 0, "opt-in": null, "version": 0}'
    ]
    @self.collector.collect_metrics('fun')
    def fun():
      pass

    fun()
    self.assert_collects_metrics()

  def test_collects_added_metrics(self):
    """Tests that we can collect custom metrics."""
    self.FileRead.side_effect = [
        '{"is-googler": true, "countdown": 0, "opt-in": null, "version": 0}'
    ]
    @self.collector.collect_metrics('fun')
    def fun():
      self.collector.add('foo', 'bar')

    fun()
    self.assert_collects_metrics({'foo': 'bar'})

  def test_collects_metrics_when_opted_in(self):
    """Tests that metrics are collected when the user opts-in."""
    self.FileRead.side_effect = [
        '{"is-googler": true, "countdown": 1234, "opt-in": true, "version": 0}'
    ]
    @self.collector.collect_metrics('fun')
    def fun():
      pass

    fun()
    self.assert_collects_metrics()

  @mock.patch('metrics.DISABLE_METRICS_COLLECTION', True)
  def test_metrics_collection_disabled(self):
    """Tests that metrics collection can be disabled via a global variable."""
    @self.collector.collect_metrics('fun')
    def fun():
      pass

    fun()

    self.assertFalse(self.collector.collecting_metrics)
    # We shouldn't have tried to read the config file.
    self.assertFalse(self.FileRead.called)
    # Nor tried to upload any metrics.
    self.assertFalse(self.Popen.called)

  def test_metrics_collection_disabled_not_googler(self):
    """Tests that metrics collection is disabled for non googlers."""
    self.FileRead.side_effect = [
        '{"is-googler": false, "countdown": 0, "opt-in": null, "version": 0}'
    ]
    @self.collector.collect_metrics('fun')
    def fun():
      pass

    fun()

    self.assertFalse(self.collector.collecting_metrics)
    self.assertFalse(self.collector.config.is_googler)
    self.assertIsNone(self.collector.config.opted_in)
    self.assertEqual(self.collector.config.countdown, 0)
    # Assert that we did not try to upload any metrics.
    self.assertFalse(self.Popen.called)

  def test_metrics_collection_disabled_opted_out(self):
    """Tests that metrics collection is disabled if the user opts out."""
    self.FileRead.side_effect = [
        '{"is-googler": true, "countdown": 0, "opt-in": false, "version": 0}'
    ]
    @self.collector.collect_metrics('fun')
    def fun():
      pass

    fun()

    self.assertFalse(self.collector.collecting_metrics)
    self.assertTrue(self.collector.config.is_googler)
    self.assertFalse(self.collector.config.opted_in)
    self.assertEqual(self.collector.config.countdown, 0)
    # Assert that we did not try to upload any metrics.
    self.assertFalse(self.Popen.called)

  def test_metrics_collection_disabled_non_zero_countdown(self):
    """Tests that metrics collection is disabled until the countdown expires."""
    self.FileRead.side_effect = [
        '{"is-googler": true, "countdown": 1, "opt-in": null, "version": 0}'
    ]
    @self.collector.collect_metrics('fun')
    def fun():
      pass

    fun()

    self.assertFalse(self.collector.collecting_metrics)
    self.assertTrue(self.collector.config.is_googler)
    self.assertFalse(self.collector.config.opted_in)
    self.assertEqual(self.collector.config.countdown, 1)
    # Assert that we did not try to upload any metrics.
    self.assertFalse(self.Popen.called)

  def test_handles_exceptions(self):
    """Tests that exception are caught and we exit with an appropriate code."""
    self.FileRead.side_effect = [
        '{"is-googler": true, "countdown": 0, "opt-in": true, "version": 0}'
    ]
    @self.collector.collect_metrics('fun')
    def fun():
      raise ValueError

    # When an exception is raised, we should catch it, update exit-code,
    # collect metrics, and re-raise it.
    with self.assertRaises(ValueError):
      fun()
    self.assert_collects_metrics({'exit_code': 1})

  def test_handles_system_exit(self):
    """Tests that the sys.exit code is respected and metrics are collected."""
    self.FileRead.side_effect = [
        '{"is-googler": true, "countdown": 0, "opt-in": true, "version": 0}'
    ]
    @self.collector.collect_metrics('fun')
    def fun():
      sys.exit(0)

    # When an exception is raised, we should catch it, update exit-code,
    # collect metrics, and re-raise it.
    with self.assertRaises(SystemExit) as cm:
      fun()
    self.assertEqual(cm.exception.code, 0)
    self.assert_collects_metrics({'exit_code': 0})

  def test_handles_system_exit_non_zero(self):
    """Tests that the sys.exit code is respected and metrics are collected."""
    self.FileRead.side_effect = [
        '{"is-googler": true, "countdown": 0, "opt-in": true, "version": 0}'
    ]
    @self.collector.collect_metrics('fun')
    def fun():
      sys.exit(123)

    # When an exception is raised, we should catch it, update exit-code,
    # collect metrics, and re-raise it.
    with self.assertRaises(SystemExit) as cm:
      fun()
    self.assertEqual(cm.exception.code, 123)
    self.assert_collects_metrics({'exit_code': 123})

  def test_prints_notice_non_zero_countdown(self):
    """Tests that a notice is printed while the countdown is non-zero."""
    self.FileRead.side_effect = [
        '{"is-googler": true, "countdown": 1234, "opt-in": null, "version": 0}'
    ]
    with self.assertRaises(SystemExit) as cm:
      with self.collector.print_notice_and_exit():
        pass
    self.assertEqual(cm.exception.code, 0)
    self.print_notice.assert_called_once_with(1234)

  def test_prints_notice_zero_countdown(self):
    """Tests that a notice is printed when the countdown reaches 0."""
    self.FileRead.side_effect = [
        '{"is-googler": true, "countdown": 0, "opt-in": null, "version": 0}'
    ]
    with self.assertRaises(SystemExit) as cm:
      with self.collector.print_notice_and_exit():
        pass
    self.assertEqual(cm.exception.code, 0)
    self.print_notice.assert_called_once_with(0)

  def test_doesnt_print_notice_opted_in(self):
    """Tests that a notice is not printed when the user opts-in."""
    self.FileRead.side_effect = [
        '{"is-googler": true, "countdown": 0, "opt-in": true, "version": 0}'
    ]
    with self.assertRaises(SystemExit) as cm:
      with self.collector.print_notice_and_exit():
        pass
    self.assertEqual(cm.exception.code, 0)
    self.assertFalse(self.print_notice.called)

  def test_doesnt_print_notice_opted_out(self):
    """Tests that a notice is not printed when the user opts-out."""
    self.FileRead.side_effect = [
        '{"is-googler": true, "countdown": 0, "opt-in": false, "version": 0}'
    ]
    with self.assertRaises(SystemExit) as cm:
      with self.collector.print_notice_and_exit():
        pass
    self.assertEqual(cm.exception.code, 0)
    self.assertFalse(self.print_notice.called)

  @mock.patch('metrics.DISABLE_METRICS_COLLECTION', True)
  def test_doesnt_print_notice_disable_metrics_collection(self):
    with self.assertRaises(SystemExit) as cm:
      with self.collector.print_notice_and_exit():
        pass
    self.assertEqual(cm.exception.code, 0)
    self.assertFalse(self.print_notice.called)
    # We shouldn't have tried to read the config file.
    self.assertFalse(self.FileRead.called)

  def test_print_notice_handles_exceptions(self):
    """Tests that exception are caught and we exit with an appropriate code."""
    self.FileRead.side_effect = [
        '{"is-googler": true, "countdown": 0, "opt-in": null, "version": 0}'
    ]
    # print_notice should catch the exception, print it and invoke sys.exit()
    with self.assertRaises(SystemExit) as cm:
      with self.collector.print_notice_and_exit():
        raise ValueError
    self.assertEqual(cm.exception.code, 1)
    self.assertTrue(self.print_notice.called)

  def test_print_notice_handles_system_exit(self):
    """Tests that the sys.exit code is respected and a notice is displayed."""
    self.FileRead.side_effect = [
        '{"is-googler": true, "countdown": 0, "opt-in": null, "version": 0}'
    ]
    # print_notice should catch the exception, print it and invoke sys.exit()
    with self.assertRaises(SystemExit) as cm:
      with self.collector.print_notice_and_exit():
        sys.exit(0)
    self.assertEqual(cm.exception.code, 0)
    self.assertTrue(self.print_notice.called)

  def test_print_notice_handles_system_exit_non_zero(self):
    """Tests that the sys.exit code is respected and a notice is displayed."""
    self.FileRead.side_effect = [
        '{"is-googler": true, "countdown": 0, "opt-in": null, "version": 0}'
    ]
    # When an exception is raised, we should catch it, update exit-code,
    # collect metrics, and re-raise it.
    with self.assertRaises(SystemExit) as cm:
      with self.collector.print_notice_and_exit():
        sys.exit(123)
    self.assertEqual(cm.exception.code, 123)
    self.assertTrue(self.print_notice.called)

  def test_counts_down(self):
    """Tests that the countdown works correctly."""
    self.FileRead.side_effect = [
        '{"is-googler": true, "countdown": 10, "opt-in": null, "version": 0}'
    ]

    # We define multiple functions to ensure it has no impact on countdown.
    @self.collector.collect_metrics('barn')
    def _barn():
      pass
    @self.collector.collect_metrics('fun')
    def _fun():
      pass
    def foo_main():
      pass

    # Assert that the countdown hasn't decrease yet.
    self.assertFalse(self.FileWrite.called)
    self.assertEqual(self.collector.config.countdown, 10)

    with self.assertRaises(SystemExit) as cm:
      with self.collector.print_notice_and_exit():
        foo_main()
    self.assertEqual(cm.exception.code, 0)

    # Assert that the countdown decreased by one, and the config file was
    # updated.
    self.assertEqual(self.collector.config.countdown, 9)
    self.print_notice.assert_called_once_with(10)

    self.assert_writes_file(
        self.config_file,
        {'is-googler': True, 'countdown': 9, 'opt-in': None, 'version': 0})

  def test_nested_functions(self):
    """Tests that a function can call another function for which metrics are
    collected."""
    self.FileRead.side_effect = [
        '{"is-googler": true, "countdown": 0, "opt-in": true, "version": 0}'
    ]

    @self.collector.collect_metrics('barn')
    def barn():
      self.collector.add('barn-metric', 1)
      return 1000
    @self.collector.collect_metrics('fun')
    def fun():
      result = barn()
      self.collector.add('fun-metric', result + 1)

    fun()

    # Assert that we collected metrics for fun, but not for barn.
    self.assert_collects_metrics({'fun-metric': 1001})

  @mock.patch('metrics.metrics_utils.CURRENT_VERSION', 5)
  def test_version_change_from_hasnt_decided(self):
    # The user has not decided yet, and the countdown hasn't reached 0, so we're
    # not collecting metrics.
    self.FileRead.side_effect = [
        '{"is-googler": true, "countdown": 9, "opt-in": null, "version": 0}'
    ]
    with self.assertRaises(SystemExit) as cm:
      with self.collector.print_notice_and_exit():
        self.collector.add('foo-metric', 1)

    self.assertEqual(cm.exception.code, 0)
    # We display the notice informing the user of the changes.
    self.print_version_change.assert_called_once_with(0)
    # But the countdown is not reset.
    self.assert_writes_file(
        self.config_file,
        {'is-googler': True, 'countdown': 8, 'opt-in': None, 'version': 0})
    # And no metrics are uploaded.
    self.assertFalse(self.Popen.called)

  @mock.patch('metrics.metrics_utils.CURRENT_VERSION', 5)
  def test_version_change_from_opted_in_by_default(self):
    # The user has not decided yet, but the countdown has reached 0, and we're
    # collecting metrics.
    self.FileRead.side_effect = [
        '{"is-googler": true, "countdown": 0, "opt-in": null, "version": 0}'
    ]
    with self.assertRaises(SystemExit) as cm:
      with self.collector.print_notice_and_exit():
        self.collector.add('foo-metric', 1)

    self.assertEqual(cm.exception.code, 0)
    # We display the notice informing the user of the changes.
    self.print_version_change.assert_called_once_with(0)
    # We reset the countdown.
    self.assert_writes_file(
        self.config_file,
        {'is-googler': True, 'countdown': 9, 'opt-in': None, 'version': 0})
    # No metrics are uploaded.
    self.assertFalse(self.Popen.called)

  @mock.patch('metrics.metrics_utils.CURRENT_VERSION', 5)
  def test_version_change_from_opted_in(self):
    # The user has opted in, and we're collecting metrics.
    self.FileRead.side_effect = [
        '{"is-googler": true, "countdown": 0, "opt-in": true, "version": 0}'
    ]
    with self.assertRaises(SystemExit) as cm:
      with self.collector.print_notice_and_exit():
        self.collector.add('foo-metric', 1)

    self.assertEqual(cm.exception.code, 0)
    # We display the notice informing the user of the changes.
    self.print_version_change.assert_called_once_with(0)
    # We reset the countdown.
    self.assert_writes_file(
        self.config_file,
        {'is-googler': True, 'countdown': 9, 'opt-in': None, 'version': 0})
    # No metrics are uploaded.
    self.assertFalse(self.Popen.called)

  @mock.patch('metrics.metrics_utils.CURRENT_VERSION', 5)
  def test_version_change_from_opted_out(self):
    # The user has opted out and we're not collecting metrics.
    self.FileRead.side_effect = [
        '{"is-googler": true, "countdown": 0, "opt-in": false, "version": 0}'
    ]
    with self.assertRaises(SystemExit) as cm:
      with self.collector.print_notice_and_exit():
        self.collector.add('foo-metric', 1)

    self.assertEqual(cm.exception.code, 0)
    # We don't display any notice.
    self.assertFalse(self.print_version_change.called)
    self.assertFalse(self.print_notice.called)
    # We don't upload any metrics.
    self.assertFalse(self.Popen.called)
    # We don't modify the config.
    self.assertFalse(self.FileWrite.called)

  @mock.patch('metrics.metrics_utils.CURRENT_VERSION', 5)
  def test_version_change_non_googler(self):
    # The user is not a googler and we're not collecting metrics.
    self.FileRead.side_effect = [
        '{"is-googler": false, "countdown": 10, "opt-in": null, "version": 0}'
    ]
    with self.assertRaises(SystemExit) as cm:
      with self.collector.print_notice_and_exit():
        self.collector.add('foo-metric', 1)

    self.assertEqual(cm.exception.code, 0)
    # We don't display any notice.
    self.assertFalse(self.print_version_change.called)
    self.assertFalse(self.print_notice.called)
    # We don't upload any metrics.
    self.assertFalse(self.Popen.called)
    # We don't modify the config.
    self.assertFalse(self.FileWrite.called)

  @mock.patch('metrics.metrics_utils.CURRENT_VERSION', 5)
  def test_opting_in_updates_version(self):
    # The user is seeing the notice telling him of the version changes.
    self.FileRead.side_effect = [
        '{"is-googler": true, "countdown": 8, "opt-in": null, "version": 0}'
    ]

    self.collector.config.opted_in = True

    # We don't display any notice.
    self.assertFalse(self.print_version_change.called)
    self.assertFalse(self.print_notice.called)
    # We don't upload any metrics.
    self.assertFalse(self.Popen.called)
    # We update the version and opt-in the user.
    self.assert_writes_file(
        self.config_file,
        {'is-googler': True, 'countdown': 8, 'opt-in': True, 'version': 5})

  @mock.patch('metrics.metrics_utils.CURRENT_VERSION', 5)
  def test_opting_in_by_default_updates_version(self):
    # The user will be opted in by default on the next execution.
    self.FileRead.side_effect = [
        '{"is-googler": true, "countdown": 1, "opt-in": null, "version": 0}'
    ]

    with self.assertRaises(SystemExit) as cm:
      with self.collector.print_notice_and_exit():
        self.collector.add('foo-metric', 1)

    self.assertEqual(cm.exception.code, 0)
    # We display the notices.
    self.print_notice.assert_called_once_with(1)
    self.print_version_change.assert_called_once_with(0)
    # We don't upload any metrics.
    self.assertFalse(self.Popen.called)
    # We update the version and set the countdown to 0. In subsequent runs,
    # we'll start collecting metrics.
    self.assert_writes_file(
        self.config_file,
        {'is-googler': True, 'countdown': 0, 'opt-in': None, 'version': 5})

  def test_add_repeated(self):
    """Tests that we can add repeated metrics."""
    self.FileRead.side_effect = [
        '{"is-googler": true, "countdown": 0, "opt-in": true}'
    ]

    @self.collector.collect_metrics('fun')
    def fun():
      self.collector.add_repeated('fun', 1)
      self.collector.add_repeated('fun', 2)
      self.collector.add_repeated('fun', 5)

    fun()

    # Assert that we collected all metrics for fun.
    self.assert_collects_metrics({'fun': [1, 2, 5]})


if __name__ == '__main__':
  unittest.main()
