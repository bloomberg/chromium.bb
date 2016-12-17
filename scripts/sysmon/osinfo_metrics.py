# Copyright 2016 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# Copyright (c) 2015 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""System metrics."""

from __future__ import print_function

import collections
import platform
import sys

from infra_libs import ts_mon

_os_name_metric = ts_mon.StringMetric(
    'proc/os/name',
    description='OS name on the machine')

_os_version_metric = ts_mon.StringMetric(
    'proc/os/version',
    description='OS version on the machine')

_os_arch_metric = ts_mon.StringMetric(
    'proc/os/arch',
    description='OS architecture on this machine')

_python_arch_metric = ts_mon.StringMetric(
    'proc/python/arch',
    description='python userland architecture on this machine')


def get_os_info():
  os_info = _get_os_info()
  _os_name_metric.set(os_info.name)
  _os_version_metric.set(os_info.version)
  _os_arch_metric.set(platform.machine())
  _python_arch_metric.set(_get_python_arch())


OSInfo = collections.namedtuple('OSInfo', 'name,version')


def _get_os_info():
  """Get OS name and version.

  Returns:
    OSInfo instance
  """
  os_name = platform.system().lower()
  os_version = ''
  if 'windows' in os_name:
    os_name = 'windows'
    # release will be something like '7', 'vista', or 'xp'
    os_version = platform.release()
  elif 'linux' in os_name:
    # will return something like ('Ubuntu', '14.04', 'trusty')
    os_name, os_version, _ = platform.dist()
  else:
    # On mac platform.system() reports 'darwin'.
    os_version = _get_mac_version()
    if os_version:
      # We found a valid mac.
      os_name = 'mac'
    else:
      # not a mac, unable to find platform information, reset
      os_name = ''
      os_version = ''

  os_name = os_name.lower()
  os_version = os_version.lower()
  return OSInfo(name=os_name, version=os_version)


def _get_mac_version():
  """Get Mac system version.

  Returns:
    Version string, which is empty if not a valid Mac system.
  """
  # This tuple is only populated on mac systems.
  mac_ver = platform.mac_ver()
  # Will be '10.11.5' or similar on a valid mac or will be '' on a non-mac.
  os_version = mac_ver[0]
  return os_version


def _get_python_arch():
  if sys.maxsize > 2**32:
    return '64'
  else:
    return '32'
