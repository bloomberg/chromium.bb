# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
import json
import logging
import shutil
import socket
import subprocess
import tempfile
import time
import urllib2

import adb_commands
import browser_backend
import browser_finder
import cros_interface
import inspector_backend

def ConstructDefaultArgsFromTheSessionManager(self, cri):
  # TODO(nduca): Fill this in with something
  pass

class CrOSBrowserBackend(browser_backend.BrowserBackend):
  """The backend for controlling a browser instance running on CrOS.
  """
  def __init__(self, browser_type, options, is_content_shell, cri):
    super(CrOSBrowserBackend, self).__init__(is_content_shell)
    # Initialize fields so that an explosion during init doesn't break in Close.
    self._options = options
    assert not is_content_shell
    self._cri = cri

    tmp = socket.socket()
    tmp.bind(('', 0))
    self._port = tmp.getsockname()[1]
    tmp.close()

    self._tmpdir = None

    self._X = None
    self._proc = None

    # TODO(nduca): Stop ui if running.
    if self._cri.IsServiceRunning('ui'):
      # Note, if this hangs, its probably because they were using wifi AND they
      # had a user-specific wifi password, which when you stop ui kills the wifi
      # connection.
      logging.debug('stopping ui')
      self._cri.GetCmdOutput(['stop', 'ui'])

    remote_port = self._cri.GetRemotePort()

    args = ['/opt/google/chrome/chrome',
            '--no-first-run',
            '--aura-host-window-use-fullscreen',
            '--remote-debugging-port=%i' % remote_port]

    if not is_content_shell:
      logging.info('Preparing user data dir')
      self._tmpdir = '/tmp/chrome_remote_control'
      if options.dont_override_profile:
        # TODO(nduca): Implement support for this.
        logging.critical('Feature not (yet) implemented.')

      # Ensure a clean user_data_dir.
      self._cri.GetCmdOutput(['rm', '-rf', self._tmpdir])

      args.append('--user-data-dir=%s' % self._tmpdir)

    # Final bits of command line prep.
    args.extend(options.extra_browser_args)
    prevent_output = not options.show_stdout

    # Stop old X.
    logging.info('Stoppping old X')
    self._cri.KillAllMatching(
      lambda name: name.startswith('/usr/bin/X '))

    # Start X.
    logging.info('Starting new X')
    X_args = ['/usr/bin/X',
              '-noreset',
              '-nolisten',
              'tcp',
              'vt01',
              '-auth',
              '/var/run/chromelogin.auth']
    self._X = cros_interface.DeviceSideProcess(
      self._cri, X_args, prevent_output=prevent_output)

    # Stop old chrome.
    logging.info('Killing old chrome')
    self._cri.KillAllMatching(
      lambda name: name.startswith('/opt/google/chrome/chrome '))

    # Start chrome via a bootstrap.
    logging.info('Starting chrome')
    self._proc = cros_interface.DeviceSideProcess(
      self._cri,
      args,
      prevent_output=prevent_output,
      extra_ssh_args=['-L%i:localhost:%i' % (self._port, remote_port)],
      leave_ssh_alive=True,
      env={'DISPLAY': ':0',
           'USER': 'chronos'},
      login_shell=True)

    # You're done.
    try:
      self._WaitForBrowserToComeUp()
    except:
      import traceback
      traceback.print_exc()
      self.Close()
      raise

  def __del__(self):
    self.Close()

  def Close(self):
    if self._proc:
      self._proc.Close()
      self._proc = None

    if self._X:
      self._X.Close()
      self._X = None

    if self._tmpdir:
      self._cri.GetCmdOutput(['rm', '-rf', self._tmpdir])
      self._tmpdir = None

    self._cri = None

  def IsBrowserRunning(self):
    if not self._proc:
      return False
    return self._proc.IsAlive()

  def CreateForwarder(self, host_port):
    assert self._cri
    return SSHReverseForwarder(self._cri, host_port)

class SSHReverseForwarder(object):
  def __init__(self, cri, host_port):
    self._proc = None

    # TODO(nduca): Try to pick a remote port that is free in a smater way. This
    # is idiotic.
    self._remote_port = cri.GetRemotePort()
    self._proc = subprocess.Popen(
      cri._FormSSHCommandLine(['sleep', '99999999999'],
                              ['-R%i:localhost:%i' %
                               (self._remote_port, host_port)]),
      stdout=subprocess.PIPE,
      stderr=subprocess.PIPE,
      stdin=subprocess.PIPE,
      shell=False)

    # TODO(nduca): How do we wait for the server to come up in a
    # robust way?
    time.sleep(1.5)

  @property
  def url(self):
    assert self._proc
    return 'http://localhost:%i' % self._remote_port

  def Close(self):
    if self._proc:
      self._proc.kill()
      self._proc = None
