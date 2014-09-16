# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import time

from metrics import Metric
from telemetry.core.platform import process_statistic_timeline_data
from telemetry.value import scalar


class PowerMetric(Metric):
  """A metric for measuring power usage."""

  # System power draw while idle.
  _quiescent_power_draw_mwh = 0

  def __init__(self, platform, quiescent_measurement_time_s=0):
    """PowerMetric Constructor.

    Args:
        platform: platform object to use.
        quiescent_measurement_time_s: time to measure quiescent power,
            in seconds. 0 means don't measure quiescent power."""
    super(PowerMetric, self).__init__()
    self._browser = None
    self._platform = platform
    self._running = False
    self._starting_cpu_stats = None
    self._results = None
    self._MeasureQuiescentPower(quiescent_measurement_time_s)

  def __del__(self):
    # TODO(jeremy): Remove once crbug.com/350841 is fixed.
    # Don't leave power monitoring processes running on the system.
    self._StopInternal()
    parent = super(PowerMetric, self)
    if hasattr(parent, '__del__'):
      parent.__del__()

  def _StopInternal(self):
    """Stop monitoring power if measurement is running. This function is
    idempotent."""
    if not self._running:
      return
    self._running = False
    self._results = self._platform.StopMonitoringPower()
    if self._results: # StopMonitoringPower() can return None.
      self._results['cpu_stats'] = (
          _SubtractCpuStats(self._browser.cpu_stats, self._starting_cpu_stats))

  def _MeasureQuiescentPower(self, measurement_time_s):
    """Measure quiescent power draw for the system."""
    if not self._platform.CanMonitorPower() or \
        self._platform.CanMeasurePerApplicationPower() or \
        not measurement_time_s:
      return

    # Only perform quiescent measurement once per run.
    if PowerMetric._quiescent_power_draw_mwh:
      return

    self._platform.StartMonitoringPower(self._browser)
    time.sleep(measurement_time_s)
    power_results = self._platform.StopMonitoringPower()
    PowerMetric._quiescent_power_draw_mwh = (
        power_results.get('energy_consumption_mwh', 0))

  def Start(self, _, tab):
    self._browser = tab.browser

    if not self._platform.CanMonitorPower():
      return

    self._results = None
    self._StopInternal()

    # This line invokes top a few times, call before starting power measurement.
    self._starting_cpu_stats = self._browser.cpu_stats
    self._platform.StartMonitoringPower(self._browser)
    self._running = True

  def Stop(self, _, tab):
    if not self._platform.CanMonitorPower():
      return

    self._StopInternal()

  def AddResults(self, _, results):
    """Add the collected power data into the results object.

    This function needs to be robust in the face of differing power data on
    various platforms. Therefore data existence needs to be checked when
    building up the results. Additionally 0 is a valid value for many of the
    metrics here which is why there are plenty of checks for 'is not None'
    below.
    """
    if not self._results:
      return

    application_energy_consumption_mwh = (
        self._results.get('application_energy_consumption_mwh'))
    total_energy_consumption_mwh = self._results.get('energy_consumption_mwh')

    if not application_energy_consumption_mwh and total_energy_consumption_mwh:
      application_energy_consumption_mwh = max(
          total_energy_consumption_mwh - PowerMetric._quiescent_power_draw_mwh,
          0)

    if total_energy_consumption_mwh is not None:
      results.AddValue(scalar.ScalarValue(
          results.current_page, 'energy_consumption_mwh', 'mWh',
          total_energy_consumption_mwh))

    if application_energy_consumption_mwh is not None:
      results.AddValue(scalar.ScalarValue(
          results.current_page, 'application_energy_consumption_mwh', 'mWh',
          application_energy_consumption_mwh))

    component_utilization = self._results.get('component_utilization', {})
    # GPU Frequency.
    gpu_power = component_utilization.get('gpu', {})
    gpu_freq_hz = gpu_power.get('average_frequency_hz')
    if gpu_freq_hz is not None:
      results.AddValue(scalar.ScalarValue(
          results.current_page, 'gpu_average_frequency_hz', 'hz', gpu_freq_hz,
          important=False))

    # Add idle wakeup numbers for all processes.
    for (process_type, stats) in self._results.get('cpu_stats', {}).items():
      trace_name_for_process = 'idle_wakeups_%s' % (process_type.lower())
      results.AddValue(scalar.ScalarValue(
          results.current_page, trace_name_for_process, 'count', stats,
          important=False))

    # Add temperature measurements.
    whole_package_utilization = component_utilization.get('whole_package', {})
    board_temperature_c = whole_package_utilization.get('average_temperature_c')
    if board_temperature_c is not None:
      results.AddValue(scalar.ScalarValue(
          results.current_page, 'board_temperature', 'celsius',
          board_temperature_c, important=False))

    # Add CPU frequency measurements.
    frequency_hz = whole_package_utilization.get('frequency_percent')
    if frequency_hz is not None:
      frequency_sum = 0.0
      for freq, percent in frequency_hz.iteritems():
        frequency_sum += freq * (percent / 100.0)
      results.AddValue(scalar.ScalarValue(
          results.current_page, 'cpu_average_frequency_hz', 'Hz',
          frequency_sum, important=False))

    # Add CPU c-state residency measurements.
    cstate_percent = whole_package_utilization.get('cstate_residency_percent')
    if cstate_percent is not None:
      for state, percent in cstate_percent.iteritems():
        results.AddValue(scalar.ScalarValue(
            results.current_page, 'cpu_cstate_%s_residency_percent' % state,
            '%', percent, important=False))

    self._results = None

def _SubtractCpuStats(cpu_stats, start_cpu_stats):
  """Computes number of idle wakeups that occurred over measurement period.

  Each of the two cpu_stats arguments is a dict as returned by the
  Browser.cpu_stats call.

  Returns:
    A dict of process type names (Browser, Renderer, etc.) to idle wakeup count
    over the period recorded by the input.
  """
  cpu_delta = {}
  total = 0
  for process_type in cpu_stats:
    assert process_type in start_cpu_stats, 'Mismatching process types'
    # Skip any process_types that are empty.
    if (not cpu_stats[process_type]) or (not start_cpu_stats[process_type]):
      continue
    # Skip if IdleWakeupCount is not present.
    if (('IdleWakeupCount' not in cpu_stats[process_type]) or
        ('IdleWakeupCount' not in start_cpu_stats[process_type])):
      continue

    assert isinstance(cpu_stats[process_type]['IdleWakeupCount'],
        process_statistic_timeline_data.IdleWakeupTimelineData)
    idle_wakeup_delta = (cpu_stats[process_type]['IdleWakeupCount'] -
                        start_cpu_stats[process_type]['IdleWakeupCount'])
    cpu_delta[process_type] = idle_wakeup_delta.total_sum()
    total = total + cpu_delta[process_type]
  cpu_delta['Total'] = total
  return cpu_delta
