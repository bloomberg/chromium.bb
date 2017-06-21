# Copyright 2017 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Process metrics."""

from __future__ import absolute_import
from __future__ import print_function

import psutil

from chromite.lib import cros_logging as logging
from infra_libs import ts_mon

logger = logging.getLogger(__name__)

_proc_count_metric = ts_mon.GaugeMetric(
    'dev/proc/count',
    description='Number of processes currently running.')
_autoserv_proc_count_metric = ts_mon.GaugeMetric(
    'dev/proc/autoserv_count',
    description='Number of autoserv processes currently running.')
_sysmon_proc_count_metric = ts_mon.GaugeMetric(
    'dev/proc/sysmon_count',
    description='Number of sysmon processes currently running.')
_apache_proc_count_metric = ts_mon.GaugeMetric(
    'dev/proc/apache_count',
    description='Number of apache processes currently running.')


def collect_proc_info():
  autoserv_count = 0
  sysmon_count = 0
  apache_count = 0
  total = 0
  for proc in psutil.process_iter():
    if _is_parent_autoserv(proc):
      autoserv_count += 1
    elif _is_sysmon(proc):
      sysmon_count += 1
    elif _is_apache(proc):
      apache_count += 1
    total += 1
  logger.debug(u'autoserv_count: %s', autoserv_count)
  logger.debug(u'sysmon_count: %s', sysmon_count)
  logger.debug(u'apache_count: %s', apache_count)
  _autoserv_proc_count_metric.set(autoserv_count)
  _sysmon_proc_count_metric.set(sysmon_count)
  _apache_proc_count_metric.set(apache_count)
  _proc_count_metric.set(total)


def _is_parent_autoserv(proc):
  """Return whether proc is a parent (not forked) autoserv process."""
  return _is_autoserv(proc) and not _is_autoserv(proc.parent())


def _is_autoserv(proc):
  """Return whether proc is an autoserv process."""
  # This relies on the autoserv script being run directly.  The script should
  # be named autoserv exactly and start with a shebang that is /usr/bin/python,
  # NOT /bin/env
  return proc.name() == 'autoserv'


def _is_apache(proc):
  """Return whether a proc is an apache2 process."""
  return proc.name() == 'apache2'


def _is_sysmon(proc):
  """Return whether proc is a sysmon process."""
  return proc.cmdline()[:3] == ['python', '-m', 'chromite.scripts.sysmon']
