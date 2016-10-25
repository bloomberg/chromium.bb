# Copyright 2016 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Send system monitoring data to the timeseries monitoring API."""

from __future__ import print_function

import random
import time

import psutil

from chromite.lib import commandline
from chromite.lib import cros_logging as logging
from chromite.lib import ts_mon_config
from chromite.scripts.sysmon import puppet_metrics
from chromite.scripts.sysmon import system_metrics
from infra_libs import ts_mon
from infra_libs.ts_mon.common import interface

logger = logging.getLogger(__name__)


class _MainLoop(object):
  """Main loop for sysmon."""

  def __init__(self, loop_func, interval=60):
    """Initialize instance.

    Args:
      loop_func: Function to call on each loop.
      interval: Time between loops in seconds.
    """
    self._loop_func = loop_func
    self._interval = interval
    self._cycles = 0

  def loop_once(self):
    """Do actions for a single loop."""
    try:
      self._loop_func(self._cycles)
    except Exception:
      logger.exception('Error during loop.')

  def loop_forever(self):
    while True:
      self.loop_once()
      time.sleep(self._interval)
      self._cycles = (self._cycles + 1) % 60


def collect_metrics(cycles):
  system_metrics.get_uptime()
  system_metrics.get_cpu_info()
  system_metrics.get_disk_info()
  system_metrics.get_mem_info()
  system_metrics.get_net_info()
  system_metrics.get_proc_info()
  if cycles == 0:
    # collect once per hour
    system_metrics.get_os_info()
  else:
    # clear on all other minutes
    system_metrics.clear_os_info()
    puppet_metrics.get_puppet_summary()
    system_metrics.get_unix_time()  # must be the last in the list


def main(args):
  parser = commandline.ArgumentParser(description=__doc__)
  parser.add_argument(
      '--interval',
      default=60, type=int,
      help='time (in seconds) between sampling system metrics')
  opts = parser.parse_args(args)
  opts.Freeze()

  # This returns a 0 value the first time it's called.  Call it now and
  # discard the return value.
  psutil.cpu_times_percent()

  # Wait a random amount of time before starting the loop in case sysmon
  # is started at exactly the same time on all machines.
  time.sleep(random.uniform(0, opts.interval))

  # This call returns a context manager that doesn't do anything, so we
  # ignore the return value.
  ts_mon_config.SetupTsMonGlobalState('sysmon')
  # The default prefix is '/chrome/infra/'.
  interface.state.metric_name_prefix = (interface.state.metric_name_prefix
                                        + 'chromeos/sysmon/')

  mainloop = _MainLoop(loop_func=collect_metrics,
                       interval=opts.interval)
  mainloop.loop_forever()
