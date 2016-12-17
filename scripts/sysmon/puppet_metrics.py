# Copyright 2016 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Puppet metrics"""

from __future__ import print_function

import time

import yaml

from chromite.lib import cros_logging as logging
from infra_libs import ts_mon

logger = logging.getLogger(__name__)

LAST_RUN_FILE = '/var/lib/puppet/state/last_run_summary.yaml'

_config_version_metric = ts_mon.GaugeMetric(
    'puppet/version/config',
    description='The version of the puppet configuration.'
    '  By default this is the time that the configuration was parsed')
_puppet_version_metric = ts_mon.StringMetric(
    'puppet/version/puppet',
    description='Version of puppet client installed.')
_events_metric = ts_mon.GaugeMetric(
    'puppet/events',
    description='Number of changes the puppet client made to the system in its'
    ' last run, by success or failure')
_resources_metric = ts_mon.GaugeMetric(
    'puppet/resources',
    description='Number of resources known by the puppet client in its last'
    ' run')
_times_metric = ts_mon.FloatMetric(
    'puppet/times',
    description='Time taken to perform various parts of the last puppet run',
    units=ts_mon.MetricsDataUnits.SECONDS)
_age_metric = ts_mon.FloatMetric(
    'puppet/age',
    description='Time since last run',
    units=ts_mon.MetricsDataUnits.SECONDS)


class _PuppetRunSummary(object):
  """Puppet run summary information."""

  def __init__(self, summary_file):
    self.filename = summary_file
    with open(self.filename) as file_:
      self._data = yaml.safe_load(file_)

  @property
  def versions(self):
    """Return mapping of version information."""
    return self._data.get('version', {})

  @property
  def config_version(self):
    """Return config version as int."""
    return self.versions.get('config', -1)

  @property
  def puppet_version(self):
    """Return Puppet version as string."""
    return self.versions.get('puppet', '')

  @property
  def events(self):
    """Return mapping of events information."""
    return self._data.get('events', {})

  @property
  def resources(self):
    """Return mapping of resources information."""
    return self._data.get('resources', {})

  @property
  def times(self):
    """Return mapping of time information."""
    return self._data.get(time, {})

  @property
  def last_run_time(self):
    """Return last run time as UNIX seconds or None."""
    return self.times.get('last_run')


def get_puppet_summary():
  """Send Puppet run summary metrics."""
  try:
    summary = _PuppetRunSummary(LAST_RUN_FILE)
  except Exception as e:
    logger.warning('Error loading Puppet run summary: %s', e)
  else:
    _config_version_metric.set(summary.config_version)
    _puppet_version_metric.set(str(summary.puppet_version))

    for key, value in summary.events.iteritems():
      _events_metric.set(value, {'result': key})

    for key, value in summary.resources.iteritems():
      _resources_metric.set(value, {'action': key})

    for key, value in summary.times.iteritems():
      _times_metric.set(value, {'step': key})

    if summary.last_run_time is not None:
      _age_metric.set(time.time() - summary.last_run_time)
