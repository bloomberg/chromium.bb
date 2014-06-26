# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import sys

from metrics import Metric
from telemetry.value import scalar


class IOMetric(Metric):
  """IO-related metrics, obtained via telemetry.core.Browser."""

  @classmethod
  def CustomizeBrowserOptions(cls, options):
    # TODO(tonyg): This is the host platform, so not totally correct.
    if sys.platform != 'darwin':
      # TODO(playmobil): Get rid of this on all platforms crbug.com/361049.
      options.AppendExtraBrowserArgs('--no-sandbox')

  def Start(self, page, tab):
    raise NotImplementedError()

  def Stop(self, page, tab):
    raise NotImplementedError()

  def AddResults(self, tab, results):
    # This metric currently only returns summary results, not per-page results.
    raise NotImplementedError()

  def AddSummaryResults(self, browser, results):
    """Add summary results to the results object."""
    io_stats = browser.io_stats
    if not io_stats['Browser']:
      return

    def AddSummariesForProcessType(process_type_io, process_type_trace):
      """For a given process type, add all relevant summary results.

      Args:
        process_type_io: Type of process (eg Browser or Renderer).
        process_type_trace: String to be added to the trace name in the results.
      """

      def AddSummaryForOperation(operation_name, trace_name_prefix, units):
        """Adds summary results for an operation in a process.

        Args:
          operation_name: The name of the operation, e.g. 'ReadOperationCount'
          trace_name_prefix: The prefix for the trace name.
        """
        if operation_name in io_stats[process_type_io]:
          value = io_stats[process_type_io][operation_name]
          if units == 'kb':
            value = value / 1024
          results.AddSummaryValue(
              scalar.ScalarValue(None, trace_name_prefix + process_type_trace,
                                 units, value, important=False))

      AddSummaryForOperation('ReadOperationCount', 'read_operations_', 'count')
      AddSummaryForOperation('WriteOperationCount', 'write_operations_',
                             'count')
      AddSummaryForOperation('ReadTransferCount', 'read_bytes_', 'kb')
      AddSummaryForOperation('WriteTransferCount', 'write_bytes_', 'kb')

    AddSummariesForProcessType('Browser', 'browser')
    AddSummariesForProcessType('Renderer', 'renderer')
    AddSummariesForProcessType('Gpu', 'gpu')
