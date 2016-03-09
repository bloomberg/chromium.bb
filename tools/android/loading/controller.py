# Copyright 2016 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Controller objects that control the context in which chrome runs.

This is responsible for the setup necessary for launching chrome, and for
creating a DevToolsConnection. There are remote device and local
desktop-specific versions.
"""

import contextlib
import datetime
import logging
import os
import shutil
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

# An estimate of time to wait for the device to become idle after expensive
# operations, such as opening the launcher activity.
_TIME_TO_DEVICE_IDLE_SECONDS = 2


class ChromeControllerBase(object):
  """Base class for all controllers.

  Defines common operations but should not be created directly.
  """
  def __init__(self):
    self._chrome_args = [
        '--disable-fre',
        '--enable-test-events',
        '--remote-debugging-port=%d' % OPTIONS.devtools_port,
    ]
    self._metadata = {}
    self._emulated_device = None
    self._emulated_network = None
    self._clear_cache = False

  def AddChromeArgument(self, arg):
    """Add command-line argument to the chrome execution."""
    self._chrome_args.append(arg)

  def SetClearCache(self, clear_cache=True):
    self._clear_cache = clear_cache

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
      network_name: (str) Key from emulation.NETWORK_CONDITIONS.
    """
    self._emulated_network = emulation.NETWORK_CONDITIONS[network_name]

  def _StartConnection(self, connection):
    """This should be called after opening an appropriate connection."""
    if self._emulated_device:
      self._metadata.update(emulation.SetUpDeviceEmulationAndReturnMetadata(
          connection, self._emulated_device))
    if self._emulated_network:
      emulation.SetUpNetworkEmulation(connection, **self._emulated_network)
      self._metadata.update(self._emulated_network)

    self._metadata.update(date=datetime.datetime.utcnow().isoformat(),
                          seconds_since_epoch=time.time())
    if self._clear_cache:
      connection.AddHook(connection.ClearCache)


class RemoteChromeController(ChromeControllerBase):
  """A controller for an android device, aka remote chrome instance."""
  # Seconds to sleep after starting chrome activity.
  POST_ACTIVITY_SLEEP_SECONDS = 2

  def __init__(self, device):
    """Initialize the controller.

    Args:
      device: an andriod device.
    """
    assert device is not None, 'Should you be using LocalController instead?'
    self._device = device
    super(RemoteChromeController, self).__init__()
    self._slow_death = False

  @contextlib.contextmanager
  def Open(self):
    """Overridden connection creation."""
    package_info = OPTIONS.ChromePackage()
    command_line_path = '/data/local/chrome-command-line'

    self._device.EnableRoot()
    self._device.KillAll(package_info.package, quiet=True)

    with device_setup.FlagReplacer(
        self._device, command_line_path, self._chrome_args):
      start_intent = intent.Intent(
          package=package_info.package, activity=package_info.activity,
          data='about:blank')
      self._device.StartActivity(start_intent, blocking=True)
      time.sleep(self.POST_ACTIVITY_SLEEP_SECONDS)
      with device_setup.ForwardPort(
          self._device, 'tcp:%d' % OPTIONS.devtools_port,
          'localabstract:chrome_devtools_remote'):
        connection = devtools_monitor.DevToolsConnection(
            OPTIONS.devtools_hostname, OPTIONS.devtools_port)
        self._StartConnection(connection)
        yield connection
    if self._slow_death:
      self._device.adb.Shell('am start com.google.android.launcher')
      time.sleep(_TIME_TO_DEVICE_IDLE_SECONDS)
      self._device.KillAll(OPTIONS.chrome_package_name, quiet=True)
      time.sleep(_TIME_TO_DEVICE_IDLE_SECONDS)
    self._device.KillAll(package_info.package, quiet=True)

  def PushBrowserCache(self, cache_path):
    """Push a chrome cache.

    Args:
      cache_path: The directory's path containing the cache locally.
    """
    chrome_cache.PushBrowserCache(self._device, cache_path)

  def PullBrowserCache(self):
    """Pull a chrome cache.

    Returns:
      Temporary directory containing all the browser cache. Caller will need to
      remove this directory manually.
    """
    return chrome_cache.PullBrowserCache(self._device)

  def SetSlowDeath(self, slow_death=True):
    """Set to pause before final kill of chrome.

    Gives time for caches to write.

    Args:
      slow_death: (bool) True if you want that which comes to all who live, to
        be slow.
    """
    self._slow_death = slow_death


class LocalChromeController(ChromeControllerBase):
  """Controller for a local (desktop) chrome instance.

  TODO(gabadie): implement cache push/pull and declare up in base class.
  """
  def __init__(self):
    super(LocalChromeController, self).__init__()
    if OPTIONS.no_sandbox:
      self.AddChromeArgument('--no-sandbox')

  @contextlib.contextmanager
  def Open(self):
    """Override for connection context."""
    binary_filename = OPTIONS.local_binary
    profile_dir = OPTIONS.local_profile_dir
    using_temp_profile_dir = profile_dir is None
    flags = self._chrome_args
    if using_temp_profile_dir:
      profile_dir = tempfile.mkdtemp()
    flags = ['--user-data-dir=%s' % profile_dir] + flags
    chrome_out = None if OPTIONS.local_noisy else file('/dev/null', 'w')
    process = subprocess.Popen(
        [binary_filename] + flags, shell=False, stderr=chrome_out)
    try:
      time.sleep(10)
      process_result = process.poll()
      if process_result is not None:
        logging.error('Unexpected process exit: %s', process_result)
      else:
        connection = devtools_monitor.DevToolsConnection(
            OPTIONS.devtools_hostname, OPTIONS.devtools_port)
        self._StartConnection(connection)
        yield connection
    finally:
      process.kill()
      if using_temp_profile_dir:
        shutil.rmtree(profile_dir)
