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
from devil.android import flag_changer
from devil.android import forwarder
from devil.android.sdk import intent

sys.path.append(os.path.join(_SRC_DIR, 'build', 'android'))
from pylib import constants

sys.path.append(os.path.join(_SRC_DIR, 'tools', 'perf'))
from chrome_telemetry_build import chromium_config

sys.path.append(chromium_config.GetTelemetryDir())
from telemetry.internal.util import webpagereplay

sys.path.append(os.path.join(_SRC_DIR, 'third_party', 'webpagereplay'))
import adb_install_cert
import certutils

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
def WprHost(device, wpr_archive_path, record=False):
  """Launches web page replay host.

  Args:
    device: Android device.
    wpr_archive_path: host sided WPR archive's path.
    record: Enables or disables WPR archive recording.

  Returns:
    Additional flags list that may be used for chromium to load web page through
    the running web page replay host.
  """
  assert device
  if wpr_archive_path == None:
    yield []
    return

  wpr_server_args = ['--use_closest_match']
  if record:
    wpr_server_args.append('--record')
    if os.path.exists(wpr_archive_path):
      os.remove(wpr_archive_path)
  else:
    assert os.path.exists(wpr_archive_path)

  # Deploy certification authority to the device.
  temp_certificate_dir = tempfile.mkdtemp()
  wpr_ca_cert_path = os.path.join(temp_certificate_dir, 'testca.pem')
  certutils.write_dummy_ca_cert(*certutils.generate_dummy_ca_cert(),
                                cert_path=wpr_ca_cert_path)

  device_cert_util = adb_install_cert.AndroidCertInstaller(
      device.adb.GetDeviceSerial(), None, wpr_ca_cert_path)
  device_cert_util.install_cert(overwrite_cert=True)
  wpr_server_args.extend(['--should_generate_certs',
                          '--https_root_ca_cert_path=' + wpr_ca_cert_path])

  # Set up WPR server and device forwarder.
  wpr_server = webpagereplay.ReplayServer(wpr_archive_path,
      '127.0.0.1', 0, 0, None, wpr_server_args)
  ports = wpr_server.StartServer()[:-1]
  host_http_port = ports[0]
  host_https_port = ports[1]

  forwarder.Forwarder.Map([(0, host_http_port), (0, host_https_port)], device)
  device_http_port = forwarder.Forwarder.DevicePortForHostPort(host_http_port)
  device_https_port = forwarder.Forwarder.DevicePortForHostPort(host_https_port)

  try:
    yield [
      '--host-resolver-rules="MAP * 127.0.0.1,EXCLUDE localhost"',
      '--testing-fixed-http-port={}'.format(device_http_port),
      '--testing-fixed-https-port={}'.format(device_https_port)]
  finally:
    forwarder.Forwarder.UnmapDevicePort(device_http_port, device)
    forwarder.Forwarder.UnmapDevicePort(device_https_port, device)
    wpr_server.StopServer()

    # Remove certification authority from the device.
    device_cert_util.remove_cert()
    shutil.rmtree(temp_certificate_dir)

@contextlib.contextmanager
def DeviceConnection(device,
                     package=DEFAULT_CHROME_PACKAGE,
                     hostname=DEVTOOLS_HOSTNAME,
                     port=DEVTOOLS_PORT,
                     host_exe='out/Release/chrome',
                     host_profile_dir=None,
                     additional_flags=None):
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
    additional_flags: Additional chromium arguments.

  Returns:
    A context manager type which evaluates to a DevToolsConnection.
  """
  package_info = constants.PACKAGE_INFO[package]
  command_line_path = '/data/local/chrome-command-line'
  new_flags = ['--disable-fre',
               '--enable-test-events',
               '--remote-debugging-port=%d' % port]
  if additional_flags != None:
    new_flags.extend(additional_flags)
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
