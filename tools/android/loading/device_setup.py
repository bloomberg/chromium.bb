# Copyright 2016 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import contextlib
import logging
import os
import shutil
import subprocess
import sys
import tempfile
import time

_SRC_DIR = os.path.abspath(os.path.join(
    os.path.dirname(__file__), '..', '..', '..'))

sys.path.append(os.path.join(_SRC_DIR, 'third_party', 'catapult', 'devil'))
from devil.android import device_utils
from devil.android.sdk import intent

sys.path.append(os.path.join(_SRC_DIR, 'build', 'android'))
from pylib import constants
from pylib import flag_changer

import devtools_monitor

DEVTOOLS_PORT = 9222
DEVTOOLS_HOSTNAME = 'localhost'
DEFAULT_CHROME_PACKAGE = 'chrome'


@contextlib.contextmanager
def TemporaryDirectory():
  """Returns a freshly-created directory that gets automatically deleted after
  usage.
  """
  name = tempfile.mkdtemp()
  try:
    yield name
  finally:
    shutil.rmtree(name)


class DeviceSetupException(Exception):
  def __init__(self, msg):
    super(DeviceSetupException, self).__init__(msg)
    logging.error(msg)


def GetFirstDevice():
  """Returns the first connected device.

  Raises:
    DeviceSetupException if there is no such device.
  """
  devices = device_utils.DeviceUtils.HealthyDevices()
  if not devices:
    raise DeviceSetupException('No devices found')
  return devices[0]


@contextlib.contextmanager
def FlagReplacer(device, command_line_path, new_flags):
  """Replaces chrome flags in a context, restores them afterwards.

  Args:
    device: Device to target, from DeviceUtils. Can be None, in which case this
      context manager is a no-op.
    command_line_path: Full path to the command-line file.
    new_flags: Flags to replace.
  """
  # If we're logging requests from a local desktop chrome instance there is no
  # device.
  if not device:
    yield
    return
  changer = flag_changer.FlagChanger(device, command_line_path)
  changer.ReplaceFlags(new_flags)
  try:
    yield
  finally:
    changer.Restore()


@contextlib.contextmanager
def ForwardPort(device, local, remote):
  """Forwards a local port to a remote one on a device in a context."""
  # If we're logging requests from a local desktop chrome instance there is no
  # device.
  if not device:
    yield
    return
  device.adb.Forward(local, remote)
  try:
    yield
  finally:
    device.adb.ForwardRemove(local)


def _SetUpDevice(device, package_info):
  """Enables root and closes Chrome on a device."""
  device.EnableRoot()
  device.KillAll(package_info.package, quiet=True)


@contextlib.contextmanager
def DeviceConnection(device,
                     package=DEFAULT_CHROME_PACKAGE,
                     hostname=DEVTOOLS_HOSTNAME,
                     port=DEVTOOLS_PORT,
                     host_exe='out/Release/chrome',
                     host_profile_dir=None):
  """Context for starting recording on a device.

  Sets up and restores any device and tracing appropriately

  Args:
    device: Android device, or None for a local run (in which case chrome needs
      to have been started with --remote-debugging-port=XXX).
    package: The key for chrome package info.
    port: The port on which to enable remote debugging.
    host_exe: The binary to execute when running on the host.
    host_profile_dir: The profile dir to use when running on the host (if None,
      a fresh profile dir will be used).

  Returns:
    A context manager type which evaluates to a DevToolsConnection.
  """
  package_info = constants.PACKAGE_INFO[package]
  command_line_path = '/data/local/chrome-command-line'
  new_flags = ['--disable-fre',
               '--enable-test-events',
               '--remote-debugging-port=%d' % port]
  if device:
    _SetUpDevice(device, package_info)
  with FlagReplacer(device, command_line_path, new_flags):
    host_process = None
    if device:
      start_intent = intent.Intent(
          package=package_info.package, activity=package_info.activity,
          data='about:blank')
      device.StartActivity(start_intent, blocking=True)
    else:
      # Run on the host.
      assert os.path.exists(host_exe)

      user_data_dir = host_profile_dir
      if not user_data_dir:
        user_data_dir = TemporaryDirectory()

      new_flags += ['--user-data-dir=%s' % user_data_dir]
      host_process = subprocess.Popen([host_exe] + new_flags,
                                      shell=False)
    if device:
      time.sleep(2)
    else:
      # TODO(blundell): Figure out why a lower sleep time causes an assertion
      # in request_track.py to fire.
      time.sleep(10)
    # If no device, we don't care about chrome startup so skip the about page.
    with ForwardPort(device, 'tcp:%d' % port,
                     'localabstract:chrome_devtools_remote'):
      yield devtools_monitor.DevToolsConnection(hostname, port)
    if host_process:
      host_process.kill()


def SetUpAndExecute(device, package, fn):
  """Start logging process.

  Wrapper for DeviceConnection for those functionally inclined.

  Args:
    device: Android device, or None for a local run.
    package: the key for chrome package info.
    fn: the function to execute that launches chrome and performs the
        appropriate instrumentation. The function will receive a
        DevToolsConnection as its sole parameter.

  Returns:
    As fn() returns.
  """
  with DeviceConnection(device, package) as connection:
    return fn(connection)
