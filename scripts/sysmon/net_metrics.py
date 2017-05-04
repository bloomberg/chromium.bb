# Copyright 2017 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Network metrics."""

from __future__ import absolute_import
from __future__ import print_function
from __future__ import unicode_literals

import collections

import psutil

from chromite.lib import cros_logging as logging
from infra_libs import ts_mon

logger = logging.getLogger(__name__)

_BOOT_TIME = psutil.boot_time()

_net_up_metric = ts_mon.CounterMetric(
    'dev/net/bytes/up', start_time=_BOOT_TIME,
    description='Number of bytes sent on interface.',
    units=ts_mon.MetricsDataUnits.BYTES)
_net_down_metric = ts_mon.CounterMetric(
    'dev/net/bytes/down', start_time=_BOOT_TIME,
    description='Number of Bytes received on '
    'interface.',
    units=ts_mon.MetricsDataUnits.BYTES)
_net_err_up_metric = ts_mon.CounterMetric(
    'dev/net/err/up', start_time=_BOOT_TIME,
    description='Total number of errors when '
    'sending (per interface).')
_net_err_down_metric = ts_mon.CounterMetric(
    'dev/net/err/down', start_time=_BOOT_TIME,
    description='Total number of errors when '
    'receiving (per interface).')
_net_drop_up_metric = ts_mon.CounterMetric(
    'dev/net/drop/up', start_time=_BOOT_TIME,
    description='Total number of outgoing '
    'packets that have been dropped.')
_net_drop_down_metric = ts_mon.CounterMetric(
    'dev/net/drop/down', start_time=_BOOT_TIME,
    description='Total number of incoming '
    'packets that have been dropped.')

_net_bytes_metric = ts_mon.CounterMetric(
    'dev/net/bytes', start_time=_BOOT_TIME,
    description='Number of bytes up/down on interface.',
    units=ts_mon.MetricsDataUnits.BYTES)
_net_packets_metric = ts_mon.CounterMetric(
    'dev/net/packets', start_time=_BOOT_TIME,
    description='Number of packets up/down on interface.',
    units=ts_mon.MetricsDataUnits.BYTES)
_net_errors_metric = ts_mon.CounterMetric(
    'dev/net/errors', start_time=_BOOT_TIME,
    description='Total number of errors up/down on interface.')
_net_dropped_metric = ts_mon.CounterMetric(
    'dev/net/dropped', start_time=_BOOT_TIME,
    description='Total number of dropped packages up/down on interface.')

_net_if_isup_metric = ts_mon.BooleanMetric(
    'dev/net/isup',
    description='Whether interface is up or down.')
_net_if_duplex_metric = ts_mon.GaugeMetric(
    'dev/net/duplex',
    description='Whether interface supports full or half duplex.')
_net_if_speed_metric = ts_mon.GaugeMetric(
    'dev/net/speed',
    description='Network interface speed in Mb.')
_net_if_mtu_metric = ts_mon.GaugeMetric(
    'dev/net/mtu',
    description='Network interface MTU in B.')


def collect_net_info():
  """Collect network metrics."""
  _collect_net_io_counters()
  _collect_net_io_duplex_counters()
  _collect_net_if_stats()


# Network IO metrics to collect
_IOMetric = collections.namedtuple('_IOMetric', ['metric', 'up_counter_name',
                                                 'down_counter_name'])

_net_io_duplex_metrics = (
  _IOMetric(metric=_net_bytes_metric,
            up_counter_name='bytes_sent',
            down_counter_name='bytes_recv'),
  _IOMetric(metric=_net_packets_metric,
            up_counter_name='packets_sent',
            down_counter_name='packets_recv'),
  _IOMetric(metric=_net_errors_metric,
            up_counter_name='errout',
            down_counter_name='errin'),
  _IOMetric(metric=_net_drop_up_metric,
            up_counter_name='dropout',
            down_counter_name='dropin'),
)


def _collect_net_io_duplex_counters():
  """Collect metrics for network IO duplex counters."""
  for nic, counters in _net_io_iter():
    fields = {'interface': nic}
    for metric, up_counter_name, down_counter_name in _net_io_duplex_metrics:
      try:
        metric.set(getattr(counters, up_counter_name),
                   fields=dict(direction='up', **fields))
        metric.set(getattr(counters, down_counter_name),
                   fields=dict(direction='down', **fields))
      except ts_mon.MonitoringDecreasingValueError as ex:
        # This normally shouldn't happen, but might if the network
        # driver module is reloaded, so log an error and continue
        # instead of raising an exception.
        logger.warning(str(ex))


_net_io_metrics = (
  (_net_up_metric, 'bytes_sent'),
  (_net_down_metric, 'bytes_recv'),
  (_net_err_up_metric, 'errout'),
  (_net_err_down_metric, 'errin'),
  (_net_drop_up_metric, 'dropout'),
  (_net_drop_down_metric, 'dropin'),
)


def _collect_net_io_counters():
  """Collect metrics for network IO counters."""
  for nic, counters in _net_io_iter():
    fields = {'interface': nic}
    for metric, counter_name in _net_io_metrics:
      try:
        metric.set(getattr(counters, counter_name), fields=fields)
      except ts_mon.MonitoringDecreasingValueError as ex:
        # This normally shouldn't happen, but might if the network
        # driver module is reloaded, so log an error and continue
        # instead of raising an exception.
        logger.warning(str(ex))


def _net_io_iter():
  """Generate network IO information."""
  nics = psutil.net_io_counters(pernic=True)
  for nic, counters in nics.iteritems():
    if _is_virtual_netif(nic):
      continue
    yield nic, counters


_net_if_metrics = (
  (_net_if_isup_metric, 'isup'),
  (_net_if_duplex_metric, 'duplex'),
  (_net_if_speed_metric, 'speed'),
  (_net_if_mtu_metric, 'mtu'),
)


def _collect_net_if_stats():
  """Collect metrics for network interface stats."""
  for nic, stats in psutil.net_if_stats().iteritems():
    if _is_virtual_netif(nic):
      continue
    fields = {'interface': nic}
    for metric, counter_name in _net_if_metrics:
      metric.set(getattr(stats, counter_name), fields=fields)


def _is_virtual_netif(nic):
    """Return whether the network interface is virtual."""
    # TODO(ayatane): Use a different way of identifying virtual interfaces
    return nic.startswith('veth')
