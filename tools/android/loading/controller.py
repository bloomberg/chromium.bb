# Copyright 2016 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Controller objects that control the context in which chrome runs.

This is responsible for the setup necessary for launching chrome, and for
creating a DevToolsConnection. There are remote device and local
desktop-specific versions.
"""

import contextlib
import logging
import os
import shutil
import socket
import subprocess
import sys
import tempfile
import time

_SRC_DIR = os.path.abspath(os.path.join(
    os.path.dirname(__file__), '..', '..', '..'))

import chrome_cache
import device_setup
import devtools_monitor
import emulation
import options

sys.path.append(os.path.join(_SRC_DIR, 'third_party', 'catapult', 'devil'))
from devil.android.sdk import intent

OPTIONS = options.OPTIONS


class ChromeControllerBase(object):
  """Base class for all controllers.

  Defines common operations but should not be created directly.
  """
  def __init__(self):
    self._chrome_args = [
        # Disable backgound network requests that may pollute WPR archive,
        # pollute HTTP cache generation, and introduce noise in loading
        # performance.
        '--disable-background-networking',
        '--disable-default-apps',
        '--no-proxy-server',
        # TODO(gabadie): Remove once crbug.com/354743 done.
        '--safebrowsing-disable-auto-update',

        # Disables actions that chrome performs only on first run or each
        # launches, which can interfere with page load performance, or even
        # block its execution by waiting for user input.
        '--disable-fre',
        '--no-default-browser-check',
        '--no-first-run',

        # Tests & dev-tools related stuff.
        '--enable-test-events',
        '--remote-debugging-port=%d' % OPTIONS.devtools_port,
    ]
    self._chrome_wpr_specific_args = []
    self._metadata = {}
    self._emulated_device = None
    self._emulated_network = None
    self._slow_death = False

  def AddChromeArgument(self, arg):
    """Add command-line argument to the chrome execution."""
    self._chrome_args.append(arg)

  @contextlib.contextmanager
  def Open(self):
    """Context that returns a connection/chrome instance.

    Returns:
      DevToolsConnection instance for which monitoring has been set up but not
      started.
    """
    raise NotImplementedError

  def ChromeMetadata(self):
    """Return metadata such as emulation information.

    Returns:
      Metadata as JSON dictionary.
    """
    return self._metadata

  def GetDevice(self):
    """Returns an android device, or None if chrome is local."""
    return None

  def SetDeviceEmulation(self, device_name):
    """Set device emulation.

    Args:
      device_name: (str) Key from --devices_file.
    """
    devices = emulation.LoadEmulatedDevices(file(OPTIONS.devices_file))
    self._emulated_device = devices[device_name]

  def SetNetworkEmulation(self, network_name):
    """Set network emulation.

    Args:
      network_name: (str) Key from emulation.NETWORK_CONDITIONS or None to
        disable network emulation.
    """
    if network_name:
      self._emulated_network = emulation.NETWORK_CONDITIONS[network_name]
    else:
      self._emulated_network = None

  def PushBrowserCache(self, cache_path):
    """Pushes the HTTP chrome cache to the profile directory.

    Caution:
      The chrome cache backend type differ according to the platform. On
      desktop, the cache backend type is `blockfile` versus `simple` on Android.
      This method assumes that your are pushing a cache with the correct backend
      type, and will NOT verify for you.

    Args:
      cache_path: The directory's path containing the cache locally.
    """
    raise NotImplementedError

  def PullBrowserCache(self):
    """Pulls the HTTP chrome cache from the profile directory.

    Returns:
      Temporary directory containing all the browser cache. Caller will need to
      remove this directory manually.
    """
    raise NotImplementedError

  def SetSlowDeath(self, slow_death=True):
    """Set to pause before final kill of chrome.

    Gives time for caches to write.

    Args:
      slow_death: (bool) True if you want that which comes to all who live, to
        be slow.
    """
    self._slow_death = slow_death

  @contextlib.contextmanager
  def OpenWprHost(self, wpr_archive_path, record=False,
                  network_condition_name=None,
                  disable_script_injection=False,
                  out_log_path=None):
    """Opens a Web Page Replay host context.

    Args:
      wpr_archive_path: host sided WPR archive's path.
      record: Enables or disables WPR archive recording.
      network_condition_name: Network condition name available in
          emulation.NETWORK_CONDITIONS.
      disable_script_injection: Disable JavaScript file injections that is
        fighting against resources name entropy.
      out_log_path: Path of the WPR host's log.
    """
    raise NotImplementedError

  def _StartConnection(self, connection):
    """This should be called after opening an appropriate connection."""
    if self._emulated_device:
      self._metadata.update(emulation.SetUpDeviceEmulationAndReturnMetadata(
          connection, self._emulated_device))
    if self._emulated_network:
      emulation.SetUpNetworkEmulation(connection, **self._emulated_network)
      self._metadata.update(self._emulated_network)

  def _GetChromeArguments(self):
    """Get command-line arguments for the chrome execution."""
    return self._chrome_args + self._chrome_wpr_specific_args


class RemoteChromeController(ChromeControllerBase):
  """A controller for an android device, aka remote chrome instance."""
  # Number of connection attempt to chrome's devtools.
  DEVTOOLS_CONNECTION_ATTEMPTS = 10

  # Time interval in seconds between chrome's devtools connection attempts.
  DEVTOOLS_CONNECTION_ATTEMPT_INTERVAL_SECONDS = 1

  # An estimate of time to wait for the device to become idle after expensive
  # operations, such as opening the launcher activity.
  TIME_TO_IDLE_SECONDS = 2

  def __init__(self, device):
    """Initialize the controller.

    Args:
      device: an andriod device.
    """
    assert device is not None, 'Should you be using LocalController instead?'
    super(RemoteChromeController, self).__init__()
    self._device = device
    self._device.EnableRoot()

  def GetDevice(self):
    """Overridden android device."""
    return self._device

  @contextlib.contextmanager
  def Open(self):
    """Overridden connection creation."""
    package_info = OPTIONS.ChromePackage()
    command_line_path = '/data/local/chrome-command-line'
    self._device.ForceStop(package_info.package)
    if OPTIONS.clear_device_data:
      logging.info('Clear Chrome data')
      self._device.adb.Shell('pm clear ' + package_info.package)
    chrome_args = self._GetChromeArguments()
    logging.info('Launching %s with flags: %s' % (package_info.package,
        subprocess.list2cmdline(chrome_args)))
    with device_setup.FlagReplacer(
        self._device, command_line_path, self._GetChromeArguments()):
      start_intent = intent.Intent(
          package=package_info.package, activity=package_info.activity,
          data='about:blank')
      self._device.StartActivity(start_intent, blocking=True)
      try:
        for attempt_id in xrange(self.DEVTOOLS_CONNECTION_ATTEMPTS + 1):
          if attempt_id == self.DEVTOOLS_CONNECTION_ATTEMPTS:
            raise RuntimeError('Failed to connect to chrome devtools after {} '
                               'attempts.'.format(attempt_id))
          logging.info('Devtools connection attempt %d' % attempt_id)
          with device_setup.ForwardPort(
              self._device, 'tcp:%d' % OPTIONS.devtools_port,
              'localabstract:chrome_devtools_remote'):
            try:
              connection = devtools_monitor.DevToolsConnection(
                  OPTIONS.devtools_hostname, OPTIONS.devtools_port)
              self._StartConnection(connection)
            except socket.error as e:
              assert str(e).startswith('[Errno 104] Connection reset by peer')
              time.sleep(self.DEVTOOLS_CONNECTION_ATTEMPT_INTERVAL_SECONDS)
              continue
            logging.info('Devtools connection success')
            yield connection
            if self._slow_death:
              self._device.adb.Shell('am start com.google.android.launcher')
              time.sleep(self.TIME_TO_IDLE_SECONDS)
            break
      finally:
        self._device.ForceStop(package_info.package)

  def PushBrowserCache(self, cache_path):
    """Override for chrome cache pushing."""
    chrome_cache.PushBrowserCache(self._device, cache_path)

  def PullBrowserCache(self):
    """Override for chrome cache pulling."""
    assert self._slow_death, 'Must do SetSlowDeath() before opening chrome.'
    return chrome_cache.PullBrowserCache(self._device)

  @contextlib.contextmanager
  def OpenWprHost(self, wpr_archive_path, record=False,
                  network_condition_name=None,
                  disable_script_injection=False,
                  out_log_path=None):
    """Starts a WPR host, overrides Chrome flags until contextmanager exit."""
    assert not self._chrome_wpr_specific_args, 'WPR is already running.'
    with device_setup.RemoteWprHost(self._device, wpr_archive_path,
        record=record,
        network_condition_name=network_condition_name,
        disable_script_injection=disable_script_injection,
        out_log_path=out_log_path) as additional_flags:
      self._chrome_wpr_specific_args = additional_flags
      yield
    self._chrome_wpr_specific_args = []


class LocalChromeController(ChromeControllerBase):
  """Controller for a local (desktop) chrome instance."""

  def __init__(self):
    super(LocalChromeController, self).__init__()
    if OPTIONS.no_sandbox:
      self.AddChromeArgument('--no-sandbox')
    self._profile_dir = OPTIONS.local_profile_dir
    self._using_temp_profile_dir = self._profile_dir is None
    if self._using_temp_profile_dir:
      self._profile_dir = tempfile.mkdtemp(suffix='.profile')
    self._headless = False

  def __del__(self):
    if self._using_temp_profile_dir:
      shutil.rmtree(self._profile_dir)

  def SetHeadless(self, headless=True):
    """Set a headless run.

    Args:
      headless: true if the chrome instance should be headless.
    """
    self._headless = headless

  @contextlib.contextmanager
  def OpenWithRedirection(self, stdout, stderr):
    """Override for connection context. stdout and stderr are passed to the
       child processes used to run Chrome and XVFB."""
    chrome_cmd = [OPTIONS.local_binary]
    chrome_cmd.extend(self._GetChromeArguments())
    chrome_cmd.append('--user-data-dir=%s' % self._profile_dir)
    chrome_cmd.extend(['--enable-logging=stderr', '--v=1'])
    # Navigates to about:blank for couples of reasons:
    #   - To find the correct target descriptor at devtool connection;
    #   - To avoid cache and WPR pollution by the NTP.
    chrome_cmd.append('about:blank')
    environment = os.environ.copy()
    if self._headless:
      environment['DISPLAY'] = 'localhost:99'
      xvfb_process = subprocess.Popen(
          ['Xvfb', ':99', '-screen', '0', '1600x1200x24'], shell=False,
          stdout=stdout, stderr=stderr)
    logging.debug(subprocess.list2cmdline(chrome_cmd))
    chrome_process = subprocess.Popen(chrome_cmd, shell=False,
                                      stdout=stdout, stderr=stderr,
                                      env=environment)
    connection = None
    try:
      time.sleep(10)
      process_result = chrome_process.poll()
      if process_result is not None:
        logging.error('Unexpected process exit: %s', process_result)
      else:
        connection = devtools_monitor.DevToolsConnection(
            OPTIONS.devtools_hostname, OPTIONS.devtools_port)
        self._StartConnection(connection)
        yield connection
        if self._slow_death:
          connection.Close()
          connection = None
          chrome_process.wait()
    finally:
      if connection:
        chrome_process.kill()
      if self._headless:
        xvfb_process.kill()

  def Open(self):
    """Wrapper around the more-specialized version of Open() above that sets
    the value of stdout/stderr based on the value of OPTIONS.local_noisy."""
    stdout = None if OPTIONS.local_noisy else file('/dev/null', 'w')
    stderr = stdout
    return self.OpenWithRedirection(stdout, stderr)

  def PushBrowserCache(self, cache_path):
    """Override for chrome cache pushing."""
    self._EnsureProfileDirectory()
    profile_cache_path = self._GetCacheDirectoryPath()
    logging.info('Copy cache directory from %s to %s.' % (
        cache_path, profile_cache_path))
    chrome_cache.CopyCacheDirectory(cache_path, profile_cache_path)

  def PullBrowserCache(self):
    """Override for chrome cache pulling."""
    cache_path = tempfile.mkdtemp()
    profile_cache_path = self._GetCacheDirectoryPath()
    logging.info('Copy cache directory from %s to %s.' % (
        profile_cache_path, cache_path))
    chrome_cache.CopyCacheDirectory(profile_cache_path, cache_path)
    return cache_path

  @contextlib.contextmanager
  def OpenWprHost(self, wpr_archive_path, record=False,
                  network_condition_name=None,
                  disable_script_injection=False,
                  out_log_path=None):
    """Override for WPR context."""
    assert not self._chrome_wpr_specific_args, 'WPR is already running.'
    with device_setup.LocalWprHost(wpr_archive_path,
        record=record,
        network_condition_name=network_condition_name,
        disable_script_injection=disable_script_injection,
        out_log_path=out_log_path) as additional_flags:
      self._chrome_wpr_specific_args = additional_flags
      yield
    self._chrome_wpr_specific_args = []

  def _EnsureProfileDirectory(self):
    if (not os.path.isdir(self._profile_dir) or
        os.listdir(self._profile_dir) == []):
      # Launch chrome so that it populates the profile directory.
      with self.Open():
        pass
      print os.listdir(self._profile_dir + '/Default')
    assert os.path.isdir(self._profile_dir)
    assert os.path.isdir(os.path.dirname(self._GetCacheDirectoryPath()))

  def _GetCacheDirectoryPath(self):
    return os.path.join(self._profile_dir, 'Default', 'Cache')
