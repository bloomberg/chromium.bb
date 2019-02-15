# Copyright 2018 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# pylint: disable=protected-access

import argparse
import unittest

import mock

from wpr import update_wpr


WPR_UPDATER = 'wpr.update_wpr.WprUpdater.'


class UpdateWPRTest(unittest.TestCase):
  def setUp(self):
    self.maxDiff = None

    self._check_log = mock.patch('core.cli_helpers.CheckLog').start()
    self._run = mock.patch('core.cli_helpers.Run').start()
    self._check_output = mock.patch('subprocess.check_output').start()
    self._check_call = mock.patch('subprocess.check_call').start()
    self._info = mock.patch('core.cli_helpers.Info').start()
    self._comment = mock.patch('core.cli_helpers.Comment').start()
    self._open = mock.patch('__builtin__.open').start()
    datetime = mock.patch('datetime.datetime').start()
    datetime.now.return_value.strftime.return_value = '<tstamp>'

    mock.patch('tempfile.mkdtemp', return_value='/tmp/dir').start()
    mock.patch('core.cli_helpers.Fatal').start()
    mock.patch('core.cli_helpers.Error').start()
    mock.patch('core.cli_helpers.Step').start()
    mock.patch('os.environ').start().copy.return_value = {}
    mock.patch('wpr.update_wpr.SRC_ROOT', '...').start()
    mock.patch('wpr.update_wpr.RESULTS2JSON', '.../results2json').start()
    mock.patch('wpr.update_wpr.HISTOGRAM2CSV', '.../histograms2csv').start()
    mock.patch('wpr.update_wpr.RUN_BENCHMARK', '.../run_benchmark').start()
    mock.patch('os.path.join', lambda *parts: '/'.join(parts)).start()

    self.wpr_updater = update_wpr.WprUpdater(argparse.Namespace(
      story='<story>', device_id=None, repeat=1, binary=None))

  def tearDown(self):
    mock.patch.stopall()

  def testMain(self):
    wpr_updater_cls = mock.patch('wpr.update_wpr.WprUpdater').start()
    update_wpr.Main([
      'live',
      '-s', 'foo:bar:story:2019',
      '-d', 'H2345234FC33',
      '--binary', '<binary>'
    ])
    self.assertListEqual(wpr_updater_cls.mock_calls, [
      mock.call(argparse.Namespace(
        binary='<binary>', command='live', device_id='H2345234FC33',
        repeat=1, story='foo:bar:story:2019')),
      mock.call().LiveRun(),
      mock.call().Cleanup(),
    ])

  def testCleanupManual(self):
    mock.patch('core.cli_helpers.Ask', return_value=False).start()
    rmtree = mock.patch('shutil.rmtree').start()
    self.wpr_updater.Cleanup()
    self._comment.assert_called_once_with(
        'No problem. All logs will remain in /tmp/dir - feel free to remove '
        'that directory when done.')
    rmtree.assert_not_called()

  def testCleanupAutomatic(self):
    mock.patch('core.cli_helpers.Ask', return_value=True).start()
    rmtree = mock.patch('shutil.rmtree').start()
    self.wpr_updater.Cleanup()
    rmtree.assert_called_once_with('/tmp/dir', ignore_errors=True)

  def testLiveRun(self):
    run_benchmark = mock.patch(
        WPR_UPDATER + '_RunSystemHealthMemoryBenchmark',
        return_value='<out-file>').start()
    print_run_info = mock.patch(WPR_UPDATER + '_PrintRunInfo').start()
    self.wpr_updater.LiveRun()
    run_benchmark.assert_called_once_with(log_name='live', live=True)
    print_run_info.assert_called_once_with('<out-file>')

  def testRunBenchmark(self):
    rename = mock.patch('os.rename').start()
    self._check_output.return_value = '  <chrome-log>'

    self.wpr_updater._RunSystemHealthMemoryBenchmark('<log_name>', True)

    # Check correct arguments when running benchmark.
    self._check_log.assert_called_once_with(
        [
          '.../run_benchmark', 'run', 'system_health.memory_desktop',
          '--browser=system', '--output-format=html', '--show-stdout',
          '--reset-results', '--story-filter=^\\<story\\>$',
          '--browser-logging-verbosity=verbose', '--pageset-repeat=1',
          '--output-dir', '/tmp/dir', '--use-live-sites'
        ],
        env={'LC_ALL': 'en_US.UTF-8'},
        log_path='/tmp/dir/<log_name>_<tstamp>')

    # Check logs are correctly extracted.
    self.assertListEqual(rename.mock_calls, [
      mock.call(
        '/tmp/dir/results.html', '/tmp/dir/<log_name>_<tstamp>.results.html'),
      mock.call('<chrome-log>', '/tmp/dir/<log_name>_<tstamp>.chrome.log'),
    ])

  def testPrintResultsHTMLInfo(self):
    self._open.return_value.__enter__.return_value.readlines.return_value = [
        'console:error:network,foo,bar',
        'console:error:js,foo,bar',
        'console:error:security,foo,bar',
    ]
    self.wpr_updater._PrintResultsHTMLInfo('<outfile>')
    self.assertListEqual(self._run.mock_calls, [
      mock.call(
        ['.../results2json', '<outfile>.results.html', '<outfile>.hist.json'],
        env={'LC_ALL': 'en_US.UTF-8'}, ok_fail=False),
      mock.call(
        ['.../histograms2csv', '<outfile>.hist.json', '<outfile>.hist.csv'],
        env={'LC_ALL': 'en_US.UTF-8'}, ok_fail=False),
    ])
    self._open.assert_called_once_with('<outfile>.hist.csv')
    self.assertListEqual(self._info.mock_calls, [
      mock.call(
        'Metrics results: file://{path}', path='<outfile>.results.html'),
      mock.call('    [console:error:network]:  bar'),
      mock.call('    [console:error:js]:       bar'),
      mock.call('    [console:error:security]: bar')
    ])

  def testPrintRunInfo(self):
    print_results = mock.patch(
        WPR_UPDATER + '_PrintResultsHTMLInfo',
        side_effect=[Exception()]).start()
    self._check_output.return_value = '0\n'
    self.wpr_updater._PrintRunInfo('<outfile>', True)
    print_results.assert_called_once_with('<outfile>')
    self.assertListEqual(self._info.mock_calls, [
      mock.call('Stdout/Stderr Log: <outfile>'),
      mock.call('Chrome Log: <outfile>.chrome.log'),
      mock.call('    Total output:   0'),
      mock.call('    Total Console:  0'),
      mock.call('    [security]:     0         grep "DevTools console '
                '.security." "<outfile>" | wc -l'),
      mock.call('    [network]:      0         grep "DevTools console '
                '.network." "<outfile>" | cut -d " " -f 20- | sort | uniq -c '
                '| sort -nr'),
    ])


if __name__ == "__main__":
  unittest.main()
