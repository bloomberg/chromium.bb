#!/usr/bin/env python
# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""A class to help start/stop a Web Page Replay Server.

The page cycler tests use this module to run Web Page Replay
(see tools/build/scripts/slave/runtest.py).

If run from the command-line, the module will launch Web Page Replay
and the specified test:

   ./webpagereplay_utils.py --help  # list options
   ./webpagereplay_utils.py 2012Q2  # run a WPR-enabled test
"""

import logging
import optparse
import os
import shutil
import signal
import subprocess
import sys
import tempfile
import time
import urllib


USAGE = '%s [options] CHROME_EXE TEST_NAME' % os.path.basename(sys.argv[0])
USER_DATA_DIR = '{TEMP}/webpagereplay_utils-chrome'

# The port numbers must match those in chrome/test/perf/page_cycler_test.cc.
HTTP_PORT = 8080
HTTPS_PORT = 8413


class ReplayError(Exception):
  """Catch-all exception for the module."""
  pass

class ReplayNotFoundError(Exception):
  pass

class ReplayNotStartedError(Exception):
  pass


class ReplayLauncher(object):
  LOG_FILE = 'log.txt'

  def __init__(self, replay_dir, archive_path, log_dir, replay_options=None):
    """Initialize ReplayLauncher.

    Args:
      replay_dir: directory that has replay.py and related modules.
      archive_path: either a directory that contains WPR archives or,
          a path to a specific WPR archive.
      log_dir: where to write log.txt.
      replay_options: a list of options strings to forward to replay.py.
    """
    self.replay_dir = replay_dir
    self.archive_path = archive_path
    self.log_dir = log_dir
    self.replay_options = replay_options if replay_options else []

    self.log_name = os.path.join(self.log_dir, self.LOG_FILE)
    self.log_fh = None
    self.proxy_process = None

    self.wpr_py = os.path.join(self.replay_dir, 'replay.py')
    if not os.path.exists(self.wpr_py):
      raise ReplayNotFoundError('Path does not exist: %s' % self.wpr_py)
    self.wpr_options = [
        '--port', str(HTTP_PORT),
        '--ssl_port', str(HTTPS_PORT),
        '--use_closest_match',
        # TODO(slamm): Add traffic shaping (requires root):
        # '--net', 'fios',
        ]
    self.wpr_options.extend(self.replay_options)

  def _OpenLogFile(self):
    if not os.path.exists(self.log_dir):
      os.makedirs(self.log_dir)
    return open(self.log_name, 'w')

  def StartServer(self):
    cmd_line = [self.wpr_py]
    cmd_line.extend(self.wpr_options)
    # TODO(slamm): Support choosing archive on-the-fly.
    cmd_line.append(self.archive_path)
    self.log_fh = self._OpenLogFile()
    logging.debug('Starting Web-Page-Replay: %s', cmd_line)
    self.proxy_process = subprocess.Popen(
      cmd_line, stdout=self.log_fh, stderr=subprocess.STDOUT)
    if not self.IsStarted():
      raise ReplayNotStartedError(
          'Web Page Replay failed to start. See the log file: ' + self.log_name)

  def IsStarted(self):
    """Checks to see if the server is up and running."""
    for _ in range(5):
      if self.proxy_process.poll() is not None:
        # The process has exited.
        break
      try:
        up_url = '%s://localhost:%s/web-page-replay-generate-200'
        http_up_url = up_url % ('http', HTTP_PORT)
        https_up_url = up_url % ('https', HTTPS_PORT)
        if (200 == urllib.urlopen(http_up_url, None, {}).getcode() and
            200 == urllib.urlopen(https_up_url, None, {}).getcode()):
          return True
      except IOError:
        time.sleep(1)
    return False

  def StopServer(self):
    if self.proxy_process:
      logging.debug('Stopping Web-Page-Replay')
      # Use a SIGINT here so that it can do graceful cleanup.
      # Otherwise, we will leave subprocesses hanging.
      self.proxy_process.send_signal(signal.SIGINT)
      self.proxy_process.wait()
    if self.log_fh:
      self.log_fh.close()


class ChromiumPaths(object):
  """Collect all the path handling together."""
  PATHS = {
      'archives':   'src/data/page_cycler/webpagereplay',
      '.wpr':       'src/data/page_cycler/webpagereplay/{TEST_NAME}.wpr',
      '.wpr_alt':   'src/tools/page_cycler/webpagereplay/tests/{TEST_NAME}.wpr',
      'start.html': 'src/tools/page_cycler/webpagereplay/start.html',
      'extension':  'src/tools/page_cycler/webpagereplay/extension',
      'replay':     'src/third_party/webpagereplay',
      'logs':       'src/webpagereplay_logs/{TEST_EXE_NAME}',
      }

  def __init__(self, **replacements):
    """Initialize ChromiumPaths.

    Args:
      replacements: a dict of format replacements for PATHS such as
          {'TEST_NAME': '2012Q2', 'TEST_EXE_NAME': 'performance_ui_tests'}.
    """
    module_dir = os.path.dirname(__file__)
    self.base_dir = os.path.abspath(os.path.join(
        module_dir, '..', '..', '..', '..'))
    self.replacements = replacements

  def __getitem__(self, key):
    path_parts = [x.format(**self.replacements)
                  for x in self.PATHS[key].split('/')]
    return os.path.join(self.base_dir, *path_parts)


def LaunchChromium(chrome_exe, chromium_paths, test_name,
                   is_dns_forwarded, use_auto):
  """Launch chromium to run WPR-backed page cycler tests.

  These options need to be kept in sync with
  src/chrome/test/perf/page_cycler_test.cc.
  """
  REPLAY_HOST='127.0.0.1'
  user_data_dir = USER_DATA_DIR.format(**{'TEMP': tempfile.gettempdir()})
  chromium_args = [
      chrome_exe,
      '--load-extension=%s' % chromium_paths['extension'],
      '--testing-fixed-http-port=%s' % HTTP_PORT,
      '--testing-fixed-https-port=%s' % HTTPS_PORT,
      '--disable-background-networking',
      '--enable-experimental-extension-apis',
      '--enable-file-cookies',
      '--enable-logging',
      '--log-level=0',
      '--enable-stats-table',
      '--enable-benchmarking',
      '--ignore-certificate-errors',
      '--metrics-recording-only',
      '--activate-on-launch',
      '--no-first-run',
      '--no-proxy-server',
      '--user-data-dir=%s' % user_data_dir,
      '--window-size=1280,1024',
      ]
  if not is_dns_forwarded:
    chromium_args.append('--host-resolver-rules=MAP * %s' % REPLAY_HOST)
  start_url = 'file://%s?test=%s' % (chromium_paths['start.html'], test_name)
  if use_auto:
    start_url += '&auto=1'
  chromium_args.append(start_url)
  if os.path.exists(user_data_dir):
    shutil.rmtree(user_data_dir)
  os.makedirs(user_data_dir)
  try:
    logging.debug('Starting Chrome: %s', chromium_args)
    retval = subprocess.call(chromium_args)
  finally:
    shutil.rmtree(user_data_dir)


def main():
  log_level = logging.DEBUG
  logging.basicConfig(level=log_level,
                      format='%(asctime)s %(filename)s:%(lineno)-3d'
                             ' %(levelname)s %(message)s',
                      datefmt='%y%m%d %H:%M:%S')

  option_parser = optparse.OptionParser(usage=USAGE)
  option_parser.add_option(
      '', '--auto', action='store_true', default=False,
      help='Start test automatically.')
  option_parser.add_option(
      '', '--replay-dir', default=None,
      help='Run replay from this directory instead of tools/build/third_party.')
  replay_group = optparse.OptionGroup(option_parser,
      'Options for replay.py', 'These options are passed through to replay.py.')
  replay_group.add_option(
      '', '--record', action='store_true', default=False,
      help='Record a new WPR archive.')
  replay_group.add_option(  # use default that does not require sudo
      '', '--dns_forwarding', default=False, action='store_true',
      help='Forward DNS requests to the local replay server.')
  option_parser.add_option_group(replay_group)
  options, args = option_parser.parse_args()
  if len(args) != 2:
    option_parser.error('Need CHROME_EXE and TEST_NAME.')
    return 1
  chrome_exe, test_name = args

  if not os.path.exists(chrome_exe):
    print >>sys.stderr, 'Chrome path does not exist:', chrome_exe
    return 1

  chromium_paths = ChromiumPaths(
      TEST_NAME=test_name,
      TEST_EXE_NAME='webpagereplay_utils')
  if os.path.exists(chromium_paths['archives']):
    archive_path = chromium_paths['.wpr']
  else:
    archive_path = chromium_paths['.wpr_alt']
  if not os.path.exists(archive_path) and not options.record:
    print >>sys.stderr, 'Archive does not exist:', archive_path
    return 1

  replay_options = []
  if options.record:
    replay_options.append('--record')
  if not options.dns_forwarding:
    replay_options.append('--no-dns_forwarding')

  if options.replay_dir:
    replay_dir = options.replay_dir
  else:
    replay_dir = chromium_paths['replay']
  wpr = ReplayLauncher(replay_dir, archive_path,
                       chromium_paths['logs'], replay_options)
  try:
    wpr.StartServer()
    LaunchChromium(chrome_exe, chromium_paths, test_name,
                   options.dns_forwarding, options.auto)
  finally:
    wpr.StopServer()
  return 0

if '__main__' == __name__:
  sys.exit(main())
