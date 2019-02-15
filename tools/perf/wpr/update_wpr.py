# Copyright 2018 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# Script to automate updating existing WPR benchmarks from live versions of the
# sites. Only supported on Mac/Linux.

import argparse
import datetime
import os
import re
import shutil
import subprocess
import tempfile

from core import cli_helpers


SRC_ROOT = os.path.abspath(
    os.path.join(os.path.dirname(__file__), '..', '..', '..'))
RESULTS2JSON = os.path.join(
    SRC_ROOT, 'third_party', 'catapult', 'tracing', 'bin', 'results2json')
HISTOGRAM2CSV = os.path.join(
    SRC_ROOT, 'third_party', 'catapult', 'tracing', 'bin', 'histograms2csv')
RUN_BENCHMARK = os.path.join(SRC_ROOT, 'tools', 'perf', 'run_benchmark')


class WprUpdater(object):
  def __init__(self, args):
    self.story = args.story
    self.device_id = args.device_id
    self.repeat = args.repeat
    self.binary = args.binary
    self.output_dir = tempfile.mkdtemp()

  def _PrepareEnv(self):
    # Enforce the same local settings for recording and replays on the bots.
    env = os.environ.copy()
    env['LC_ALL'] = 'en_US.UTF-8'
    return env

  def _Run(self, command, ok_fail=False):
    return cli_helpers.Run(command, ok_fail=ok_fail, env=self._PrepareEnv())

  def _CheckLog(self, command, log_name):
    # This is a wrapper around cli_helpers.CheckLog that adds timestamp to the
    # log filename and substitutes placeholders such as {src}, {name},
    # {device_id} in the command.
    name_regex = '^%s$' % re.escape(self.story)
    command = [
        c.format(src=SRC_ROOT, name=name_regex, device_id=self.device_id)
        for c in command]
    timestamp = datetime.datetime.now().strftime('%Y_%m_%d_%H_%M_%S')
    log_path = os.path.join(self.output_dir, '%s_%s' % (log_name, timestamp))
    cli_helpers.CheckLog(command, log_path=log_path, env=self._PrepareEnv())
    return log_path

  def _IsDesktop(self):
    return self.device_id is None

  def _ExtractLogFile(self, out_file):
    # This method extracts the name of the chrome log file from the
    # run_benchmark output log and copies it to the temporary directory next to
    # the log file, which ensures that it is not overridden by the next run.
    try:
      line = subprocess.check_output(
        ['grep', 'Chrome log file will be saved in', out_file])
      os.rename(line.split()[-1], out_file + '.chrome.log')
    except subprocess.CalledProcessError as e:
      cli_helpers.Error('Could not find log file: {error}', error=e)

  def _ExtractResultsFile(self, out_file):
    results_file = out_file + '.results.html'
    os.rename(os.path.join(self.output_dir, 'results.html'), results_file)

  def _RunSystemHealthMemoryBenchmark(self, log_name, live=False):
    args = [RUN_BENCHMARK, 'run']

    if self._IsDesktop():
      args.append('system_health.memory_desktop')
    else:
      args.extend('system_health.memory_mobile')

    if self.binary:
      args.extend([
        '--browser-executable=%s' % self.binary,
        '--browser=exact',
      ])
    elif self._IsDesktop():
      args.append('--browser=system')
    else:
      args.append('--browser=android-system-chrome')

    args.extend([
      '--output-format=html', '--show-stdout',
      '--reset-results', '--story-filter={name}',
      '--browser-logging-verbosity=verbose',
      '--pageset-repeat=%s' % self.repeat,
      '--output-dir', self.output_dir])
    if live:
      args.append('--use-live-sites')
    out_file = self._CheckLog(args, log_name=log_name)
    self._ExtractResultsFile(out_file)
    self._ExtractLogFile(out_file)
    return out_file

  def _PrintResultsHTMLInfo(self, out_file):
    results_file = out_file + '.results.html'
    histogram_json = out_file + '.hist.json'
    histogram_csv = out_file + '.hist.csv'

    self._Run([RESULTS2JSON, results_file, histogram_json])
    self._Run([HISTOGRAM2CSV, histogram_json, histogram_csv])

    cli_helpers.Info('Metrics results: file://{path}', path=results_file)
    names = set([
        'console:error:network',
        'console:error:js',
        'console:error:all',
        'console:error:security'])
    with open(histogram_csv) as f:
      for line in f.readlines():
        line = line.split(',')
        if line[0] in names:
          cli_helpers.Info('    %-26s%s' % ('[%s]:' % line[0], line[2]))

  def _PrintRunInfo(self, out_file, results_details=True):
    try:
      if results_details:
        self._PrintResultsHTMLInfo(out_file)
    except Exception as e:
      cli_helpers.Error('Could not print results.html tests: %s' % e)

    def shell(cmd):
      return subprocess.check_output(cmd, shell=True).rstrip()

    def statsFor(name, filters='wc -l'):
      cmd = 'grep "DevTools console .%s." "%s"' % (name, out_file)
      cmd += ' | ' + filters
      output = shell(cmd) or '0'
      if len(output) > 7:
        cli_helpers.Info('    %-26s%s' % ('[%s]:' % name, cmd))
        cli_helpers.Info('      ' + output.replace('\n', '\n      '))
      else:
        cli_helpers.Info('    %-16s%-8s  %s' % ('[%s]:' % name, output, cmd))

    cli_helpers.Info('Stdout/Stderr Log: %s' % out_file)
    cli_helpers.Info('Chrome Log: %s.chrome.log' % out_file)
    cli_helpers.Info(
        '    Total output:   %s' %
        subprocess.check_output(['wc', '-l', out_file]).rstrip())
    cli_helpers.Info(
        '    Total Console:  %s' %
        shell('grep "DevTools console" "%s" | wc -l' % out_file))
    statsFor('security')
    statsFor('network', 'cut -d " " -f 20- | sort | uniq -c | sort -nr')

    chrome_log = '%s.chrome.log' % out_file
    if os.path.isfile(chrome_log):
      cmd = 'grep "Uncaught .*Error" "%s"' % chrome_log
      count = shell(cmd + '| wc -l')
      cli_helpers.Info('    %-16s%-8s  %s' % ('[javascript]:', count, cmd))

  def LiveRun(self):
    cli_helpers.Step('LIVE RUN: %s' % self.story)
    out_file = self._RunSystemHealthMemoryBenchmark(
        log_name='live', live=True)
    self._PrintRunInfo(out_file)
    return out_file

  def Cleanup(self):
    if cli_helpers.Ask('Should I clean up the temp dir with logs?'):
      shutil.rmtree(self.output_dir, ignore_errors=True)
    else:
      cli_helpers.Comment(
        'No problem. All logs will remain in %s - feel free to remove that '
        'directory when done.' % self.output_dir)


def Main(argv):
  parser = argparse.ArgumentParser()
  parser.add_argument(
      '-s', '--story', dest='story', required=True,
      help='Benchmark story to be recorded, replayed or uploaded.')
  parser.add_argument(
      '-d', '--device-id', dest='device_id',
      help='Specify the device serial number listed by `adb devices`. When not '
           'specified, the script runs in desktop mode.')
  parser.add_argument(
      '--pageset-repeat', type=int, default=1, dest='repeat',
      help='Number of times to repeat the entire pageset.')
  parser.add_argument(
      '--binary', default=None,
      help='Path to the Chromium/Chrome binary relative to output directory. '
           'Defaults to default Chrome browser installed if not specified.')
  parser.add_argument(
      'command', choices=['live'],
      help='Mode in which to run this script.')

  args = parser.parse_args(argv)

  updater = WprUpdater(args)
  if args.command =='live':
    updater.LiveRun()
  updater.Cleanup()
