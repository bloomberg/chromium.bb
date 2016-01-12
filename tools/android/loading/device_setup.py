# Copyright 2016 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import contextlib
import os
import sys
import time

file_dir = os.path.dirname(__file__)
sys.path.append(os.path.join(file_dir, '..', '..', '..', 'build', 'android'))

from pylib import constants
from pylib import flag_changer
from pylib.device import device_utils
from pylib.device import intent

DEVTOOLS_PORT = 9222
DEVTOOLS_HOSTNAME = 'localhost'

@contextlib.contextmanager
def FlagChanger(device, command_line_path, new_flags):
  """Changes the flags in a context, restores them afterwards.

  Args:
    device: Device to target, from DeviceUtils.
    command_line_path: Full path to the command-line file.
    new_flags: Flags to add.
  """
  # If we're logging requests from a local desktop chrome instance there is no
  # device.
  if not device:
    yield
    return
  changer = flag_changer.FlagChanger(device, command_line_path)
  changer.AddFlags(new_flags)
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


def SetUpAndExecute(device, package, fn):
  """Start logging process.

  Sets up any device and tracing appropriately and then executes the core
  logging function.

  Args:
    device: Android device, or None for a local run.
    package: the key for chrome package info.
    fn: the function to execute that launches chrome and performs the
        appropriate instrumentation, see _Log*Internal().

  Returns:
    As fn() returns.
  """
  package_info = constants.PACKAGE_INFO[package]
  command_line_path = '/data/local/chrome-command-line'
  new_flags = ['--enable-test-events',
               '--remote-debugging-port=%d' % DEVTOOLS_PORT]
  if device:
    _SetUpDevice(device, package_info)
  with FlagChanger(device, command_line_path, new_flags):
    if device:
      start_intent = intent.Intent(
          package=package_info.package, activity=package_info.activity,
          data='about:blank')
      device.StartActivity(start_intent, blocking=True)
      time.sleep(2)
    # If no device, we don't care about chrome startup so skip the about page.
    with ForwardPort(device, 'tcp:%d' % DEVTOOLS_PORT,
                     'localabstract:chrome_devtools_remote'):
      return fn()
