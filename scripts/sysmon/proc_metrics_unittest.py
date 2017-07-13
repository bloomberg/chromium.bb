# Copyright 2017 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Unit tests for proc_metrics."""

# pylint: disable=protected-access

from __future__ import absolute_import
from __future__ import print_function

import mock
import psutil

from chromite.lib import cros_test_lib
from chromite.scripts.sysmon import proc_metrics


def _mock_process(name, cmdline, parent=None):
  proc = mock.Mock(dir(psutil.Process))
  proc.name.return_value = name
  proc.cmdline.return_value = cmdline
  proc.cpu_percent.return_value = 2
  if parent is not None:
    proc.parent.return_value = parent
  return proc


def _mock_forked_process(name, cmdline):
  parent_proc = _mock_process(name, cmdline)
  return _mock_process(name, cmdline, parent=parent_proc)


class TestProcMetrics(cros_test_lib.TestCase):
  """Tests for proc_metrics."""

  def setUp(self):
    patcher = mock.patch('infra_libs.ts_mon.common.interface.state.store',
                         autospec=True)
    self.store = patcher.start()
    self.addCleanup(patcher.stop)

  def test_collect(self):
    with mock.patch('psutil.process_iter', autospec=True) as process_iter:
      process_iter.return_value = [
          _mock_process(
              name='autoserv',
              cmdline=['/usr/bin/python',
                       '-u', '/usr/local/autotest/server/autoserv',
                       '-p',
                       '-r', ('/usr/local/autotest/results/hosts/'
                              'chromeos4-row3-rack13-host9/646252-provision'
                              '/20171307125911'),
                       '-m', 'chromeos4-row3-rack13-host9',
                       '--verbose', '--lab', 'True',
                       '--provision', '--job-labels',
                       'cros-version:winky-release/R61-9741.0.0']
          ),
          _mock_process(
              name='apache2',
              cmdline=['/usr/sbin/apache2', '-k', 'start'],
          ),
          _mock_forked_process(
              name='autoserv',
              cmdline=['/usr/bin/python',
                       '-u', '/usr/local/autotest/server/autoserv',
                       '-p',
                       '-r', ('/usr/local/autotest/results/hosts/'
                              'chromeos4-row3-rack13-host9/646252-provision'
                              '/20171307125911'),
                       '-m', 'chromeos4-row3-rack13-host9',
                       '--verbose', '--lab', 'True',
                       '--provision', '--job-labels',
                       'cros-version:winky-release/R61-9741.0.0']
          ),
          _mock_process(
              name='python',
              cmdline=[('/usr/local/google/home/chromeos-test/.cache/cros_venv'
                        '/venv-2.7.6-5addca6cf590166d7b70e22a95bea4a0'
                        '/bin/python'),
                       '-m', 'chromite.scripts.sysmon', '--interval', '60']
          ),
      ]
      proc_metrics.collect_proc_info()

    setter = self.store.set
    calls = [
        mock.call('proc/count', ('autoserv',), None,
                  1, enforce_ge=mock.ANY),
        mock.call('proc/cpu_percent', ('autoserv',), None,
                  2, enforce_ge=mock.ANY),
        mock.call('proc/count', ('sysmon',), None,
                  1, enforce_ge=mock.ANY),
        mock.call('proc/cpu_percent', ('sysmon',), None,
                  2, enforce_ge=mock.ANY),
        mock.call('proc/count', ('apache',), None,
                  1, enforce_ge=mock.ANY),
        mock.call('proc/cpu_percent', ('apache',), None,
                  2, enforce_ge=mock.ANY),
        mock.call('proc/count', ('other',), None,
                  1, enforce_ge=mock.ANY),
        mock.call('proc/cpu_percent', ('other',), None,
                  2, enforce_ge=mock.ANY),
    ]
    setter.assert_has_calls(calls)
    self.assertEqual(len(setter.mock_calls), len(calls))
