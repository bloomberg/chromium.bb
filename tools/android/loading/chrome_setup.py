# Copyright 2016 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Handles Chrome's configuration."""

import contextlib
import json
import shutil
import subprocess
import tempfile
import time

import devtools_monitor
from options import OPTIONS


# Copied from
# WebKit/Source/devtools/front_end/network/NetworkConditionsSelector.js
_NETWORK_CONDITIONS = {
    'Offline': {
        'download': 0 * 1024 / 8, 'upload': 0 * 1024 / 8, 'latency': 0},
    'GPRS': {
        'download': 50 * 1024 / 8, 'upload': 20 * 1024 / 8, 'latency': 500},
    'Regular 2G': {
        'download': 250 * 1024 / 8, 'upload': 50 * 1024 / 8, 'latency': 300},
    'Good 2G': {
        'download': 450 * 1024 / 8, 'upload': 150 * 1024 / 8, 'latency': 150},
    'Regular 3G': {
        'download': 750 * 1024 / 8, 'upload': 250 * 1024 / 8, 'latency': 100},
    'Good 3G': {
        'download': 1.5 * 1024 * 1024 / 8, 'upload': 750 * 1024 / 8,
        'latency': 40},
    'Regular 4G': {
        'download': 4 * 1024 * 1024 / 8, 'upload': 3 * 1024 * 1024 / 8,
        'latency': 20},
    'DSL': {
        'download': 2 * 1024 * 1024 / 8, 'upload': 1 * 1024 * 1024 / 8,
        'latency': 5},
    'WiFi': {
        'download': 30 * 1024 * 1024 / 8, 'upload': 15 * 1024 * 1024 / 8,
        'latency': 2}
}


@contextlib.contextmanager
def DevToolsConnectionForLocalBinary(flags):
  """Returns a DevToolsConnection context manager for a local binary.

  Args:
    flags: ([str]) List of flags to pass to the browser.

  Returns:
    A DevToolsConnection context manager.
  """
  binary_filename = OPTIONS.local_binary
  profile_dir = OPTIONS.local_profile_dir
  temp_profile_dir = profile_dir is None
  if temp_profile_dir:
    profile_dir = tempfile.mkdtemp()
  flags.append('--user-data-dir=%s' % profile_dir)
  chrome_out = None if OPTIONS.local_noisy else file('/dev/null', 'w')
  process = subprocess.Popen(
      [binary_filename] + flags, shell=False, stderr=chrome_out)
  try:
    time.sleep(10)
    yield devtools_monitor.DevToolsConnection(
        OPTIONS.devtools_hostname, OPTIONS.devtools_port)
  finally:
    process.kill()
    if temp_profile_dir:
      shutil.rmtree(temp_profile_dir)


def SetUpEmulationAndReturnMetadata(connection, emulated_device_name,
                                    emulated_network_name):
  """Sets up the device and network emulation and returns the trace metadata.

  Args:
    connection: (DevToolsConnection)
    emulated_device_name: (str) Key in the dict returned by
                          _LoadEmulatedDevices().
    emulated_network_name: (str) Key in _NETWORK_CONDITIONS.

  Returns:
    A metadata dict {'deviceEmulation': params, 'networkEmulation': params}.
  """
  result = {'deviceEmulation': {}, 'networkEmulation': {}}
  if emulated_device_name:
    devices = _LoadEmulatedDevices(OPTIONS.devices_file)
    emulated_device = devices[emulated_device_name]
    emulation_params = _SetUpDeviceEmulationAndReturnMetadata(
        connection, emulated_device)
    result['deviceEmulation'] = emulation_params
  if emulated_network_name:
    params = _NETWORK_CONDITIONS[emulated_network_name]
    _SetUpNetworkEmulation(
        connection, params['latency'], params['download'], params['upload'])
    result['networkEmulation'] = params
  return result


def _LoadEmulatedDevices(filename):
  """Loads a list of emulated devices from the DevTools JSON registry.

  Args:
    filename: (str) Path to the JSON file.

  Returns:
    {'device_name': device}
  """
  json_dict = json.load(open(filename, 'r'))
  devices = {}
  for device in json_dict['extensions']:
    device = device['device']
    devices[device['title']] = device
  return devices


def _GetDeviceEmulationMetadata(device):
  """Returns the metadata associated with a given device."""
  return {'width': device['screen']['vertical']['width'],
          'height': device['screen']['vertical']['height'],
          'deviceScaleFactor': device['screen']['device-pixel-ratio'],
          'mobile': 'mobile' in device['capabilities'],
          'userAgent': device['user-agent']}


def _SetUpDeviceEmulationAndReturnMetadata(connection, device):
  """Configures an instance of Chrome for device emulation.

  Args:
    connection: (DevToolsConnection)
    device: (dict) As returned by LoadEmulatedDevices().

  Returns:
    A dict containing the device emulation metadata.
  """
  print device
  res = connection.SyncRequest('Emulation.canEmulate')
  assert res['result'], 'Cannot set device emulation.'
  data = _GetDeviceEmulationMetadata(device)
  connection.SyncRequestNoResponse(
      'Emulation.setDeviceMetricsOverride',
      {'width': data['width'],
       'height': data['height'],
       'deviceScaleFactor': data['deviceScaleFactor'],
       'mobile': data['mobile'],
       'fitWindow': True})
  connection.SyncRequestNoResponse('Network.setUserAgentOverride',
                                   {'userAgent': data['userAgent']})
  return data


def _SetUpNetworkEmulation(connection, latency, download, upload):
  """Configures an instance of Chrome for network emulation.

  Args:
    connection: (DevToolsConnection)
    latency: (float) Latency in ms.
    download: (float) Download speed (Bytes / s).
    upload: (float) Upload speed (Bytes / s).
  """
  res = connection.SyncRequest('Network.canEmulateNetworkConditions')
  assert res['result'], 'Cannot set network emulation.'
  connection.SyncRequestNoResponse(
      'Network.emulateNetworkConditions',
      {'offline': False, 'latency': latency, 'downloadThroughput': download,
       'uploadThroughput': upload})
