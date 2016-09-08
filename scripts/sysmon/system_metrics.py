# Copyright 2016 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# Copyright (c) 2015 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""System metrics."""

from __future__ import print_function

import errno
import os
import platform
import sys
import time

import psutil

from chromite.lib import cros_logging as logging
from infra_libs import ts_mon


cpu_count = ts_mon.GaugeMetric('dev/cpu/count',
                               description='Number of CPU cores.')
cpu_time = ts_mon.FloatMetric('dev/cpu/time',
                              description='percentage of time spent by the CPU'
                              ' in different states.')

disk_free = ts_mon.GaugeMetric(
    'dev/disk/free',
    description='Available bytes on disk partition.',
    units=ts_mon.MetricsDataUnits.BYTES)
disk_total = ts_mon.GaugeMetric('dev/disk/total',
                                description='Total bytes on disk partition.',
                                units=ts_mon.MetricsDataUnits.BYTES)

# inode counts are only available on Unix.
if os.name == 'posix':  # pragma: no cover
  inodes_free = ts_mon.GaugeMetric(
      'dev/inodes/free',
      description='Number of available inodes on '
      'disk partition (unix only).')
  inodes_total = ts_mon.GaugeMetric(
      'dev/inodes/total',
      description='Number of possible inodes on '
      'disk partition (unix only)')

mem_free = ts_mon.GaugeMetric('dev/mem/free',
                              description='Amount of memory available to a '
                              'process (in Bytes). Buffers are considered '
                              'free memory.',
                              units=ts_mon.MetricsDataUnits.BYTES)

mem_total = ts_mon.GaugeMetric('dev/mem/total',
                               description='Total physical memory in Bytes.',
                               units=ts_mon.MetricsDataUnits.BYTES)

START_TIME = psutil.boot_time()
net_up = ts_mon.CounterMetric('dev/net/bytes/up', start_time=START_TIME,
                              description='Number of bytes sent on interface.',
                              units=ts_mon.MetricsDataUnits.BYTES)
net_down = ts_mon.CounterMetric('dev/net/bytes/down', start_time=START_TIME,
                                description='Number of Bytes received on '
                                'interface.',
                                units=ts_mon.MetricsDataUnits.BYTES)
net_err_up = ts_mon.CounterMetric('dev/net/err/up', start_time=START_TIME,
                                  description='Total number of errors when '
                                  'sending (per interface).')
net_err_down = ts_mon.CounterMetric('dev/net/err/down', start_time=START_TIME,
                                    description='Total number of errors when '
                                    'receiving (per interface).')
net_drop_up = ts_mon.CounterMetric('dev/net/drop/up', start_time=START_TIME,
                                   description='Total number of outgoing '
                                   'packets that have been dropped.')
net_drop_down = ts_mon.CounterMetric(
    'dev/net/drop/down', start_time=START_TIME,
    description='Total number of incoming '
    'packets that have been dropped.')

disk_read = ts_mon.CounterMetric('dev/disk/read', start_time=START_TIME,
                                 description='Number of Bytes read on disk.',
                                 units=ts_mon.MetricsDataUnits.BYTES)
disk_write = ts_mon.CounterMetric('dev/disk/write', start_time=START_TIME,
                                  description='Number of Bytes written on '
                                  'disk.',
                                  units=ts_mon.MetricsDataUnits.BYTES)

uptime = ts_mon.GaugeMetric('dev/uptime',
                            description='Machine uptime, in seconds.',
                            units=ts_mon.MetricsDataUnits.SECONDS)

proc_count = ts_mon.GaugeMetric('dev/proc/count',
                                description='Number of processes currently '
                                'running.')
load_average = ts_mon.FloatMetric('dev/proc/load_average',
                                  description='Number of processes currently '
                                  'in the system run queue.')

# tsmon pipeline uses backend clocks when assigning timestamps to metric points.
# By comparing point timestamp to the point value (i.e. time by machine's local
# clock), we can potentially detect some anomalies (clock drift, unusually high
# metrics pipeline delay, completely wrong clocks, etc).
#
# It is important to gather this metric right before the flush.
unix_time = ts_mon.GaugeMetric(
    'dev/unix_time',
    description='Number of milliseconds since epoch '
    'based on local machine clock.')

os_name = ts_mon.StringMetric('proc/os/name',
                              description='OS name on the machine ')

os_version = ts_mon.StringMetric('proc/os/version',
                                 description='OS version on the machine ')

os_arch = ts_mon.StringMetric('proc/os/arch',
                              description='OS architecture on this machine')

python_arch = ts_mon.StringMetric('proc/python/arch',
                                  description='python userland '
                                  'architecture on this machine')


def get_uptime():
  uptime.set(int(time.time() - START_TIME))


def get_cpu_info():
  cpu_count.set(psutil.cpu_count())

  times = psutil.cpu_times_percent()
  for mode in ('user', 'system', 'idle'):
    cpu_time.set(getattr(times, mode), {'mode': mode})


def get_disk_info(mountpoints=None):
  if mountpoints is None:
    mountpoints = [disk.mountpoint for disk in psutil.disk_partitions()]

  for mountpoint in mountpoints:
    fields = {'path': mountpoint}

    try:
      usage = psutil.disk_usage(mountpoint)
    except OSError as ex:
      if ex.errno == errno.ENOENT:
        # This happens on Windows when querying a removable drive that
        # doesn't have any media inserted right now.
        continue
      raise  # pragma: no cover

    disk_free.set(usage.free, fields=fields)
    disk_total.set(usage.total, fields=fields)

    # inode counts are only available on Unix.
    if os.name == 'posix':  # pragma: no cover
      stats = os.statvfs(mountpoint)
      inodes_free.set(stats.f_favail, fields=fields)
      inodes_total.set(stats.f_files, fields=fields)

  try:
    for disk, counters in psutil.disk_io_counters(perdisk=True).iteritems():
      fields = {'disk': disk}
      disk_read.set(counters.read_bytes, fields=fields)
      disk_write.set(counters.write_bytes, fields=fields)
  except RuntimeError as ex:
    if "couldn't find any physical disk" in str(ex):
      # Disk performance counters aren't enabled on Windows.
      pass
    else:
      raise


def get_mem_info():
  # We don't report mem.used because (due to virtual memory) it is not
  # useful.
  mem = psutil.virtual_memory()
  mem_free.set(mem.available)
  mem_total.set(mem.total)


def get_net_info():
  metric_counter_names = [
      (net_up, 'bytes_sent'),
      (net_down, 'bytes_recv'),
      (net_err_up, 'errout'),
      (net_err_down, 'errin'),
      (net_drop_up, 'dropout'),
      (net_drop_down, 'dropin'),
  ]

  nics = psutil.net_io_counters(pernic=True)
  for nic, counters in nics.iteritems():
    fields = {'interface': nic}
    for metric, counter_name in metric_counter_names:
      try:
        metric.set(getattr(counters, counter_name), fields=fields)
      except ts_mon.MonitoringDecreasingValueError as ex:
        # This normally shouldn't happen, but might if the network
        # driver module is reloaded, so log an error and continue
        # instead of raising an exception.
        logging.error(str(ex))


def get_os_info():
  os_name_data = ''
  os_version_data = ''

  os_name_data = platform.system().lower()
  if 'windows' in os_name_data:
    os_name_data = 'windows'
    # os_release will be something like '7', 'vista', or 'xp'
    os_version_data = platform.release()

  elif 'linux' in os_name_data:
    # will return something like ('Ubuntu', '14.04', 'trusty')
    dist_info_data = platform.dist()
    os_name_data = dist_info_data[0]
    os_version_data = dist_info_data[1]

  # on mac platform.system() reports 'darwin'
  else:
    # this tuple is only populated on mac systems
    mac_ver_data = platform.mac_ver()
    # [0] will be '10.11.5' or similar on a valid mac or will be '' on a
    # non-mac
    os_version_data = mac_ver_data[0]
    if os_version_data:
      # we found a valid mac
      os_name_data = 'mac'
    else:
      # not a mac, unable to find platform information, reset
      os_name_data = ''
      os_version_data = ''

  # normalize to lower case
  os_name_data = os_name_data.lower()
  os_version_data = os_version_data.lower()

  python_arch_data = '32'
  if sys.maxsize > 2**32:
    python_arch_data = '64'

  # construct metrics
  os_name.set(os_name_data)
  os_version.set(os_version_data)
  os_arch.set(platform.machine())
  python_arch.set(python_arch_data)


def clear_os_info():
  os_name.reset()
  os_version.reset()
  os_arch.reset()
  python_arch.reset()


def get_proc_info():
  procs = psutil.pids()
  proc_count.set(len(procs))

  if os.name == 'posix':  # pragma: no cover
    try:
      avg1, avg5, avg15 = os.getloadavg()
    except OSError:  # pragma: no cover
      pass
    else:
      load_average.set(avg1, fields={'minutes': 1})
      load_average.set(avg5, fields={'minutes': 5})
      load_average.set(avg15, fields={'minutes': 15})


def get_unix_time():
  unix_time.set(int(time.time() * 1000))
