# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
import sys

class MemoryMetrics(object):
  def __init__(self):
    self._start_commit_charge = None

  def Start(self, browser):
    self._start_commit_charge = browser.memory_stats['SystemCommitCharge']

  def StopAndGetResults(self, browser, results):
    memory = browser.memory_stats
    if not memory['Browser']:
      return

    metric = 'resident_set_size'
    if sys.platform == 'win32':
      metric = 'working_set'

    def AddSummariesForProcessTypes(process_types_memory, process_type_trace):
      def AddSummary(value_name_memory, value_name_trace):
        if len(process_types_memory) > 1 and value_name_memory.endswith('Peak'):
          return
        values = []
        for process_type_memory in process_types_memory:
          if value_name_memory in memory[process_type_memory]:
            values.append(memory[process_type_memory][value_name_memory])
        if values:
          results.AddSummary(value_name_trace + process_type_trace,
                             'bytes', sum(values), data_type='unimportant')
      AddSummary('VM', 'vm_final_size_')
      AddSummary('WorkingSetSize', 'vm_%s_final_size_' % metric)
      AddSummary('PrivateDirty', 'vm_private_dirty_final_')
      AddSummary('ProportionalSetSize', 'vm_proportional_set_size_final_')
      AddSummary('VMPeak', 'vm_peak_size_')
      AddSummary('WorkingSetSizePeak', '%s_peak_size_' % metric)

    AddSummariesForProcessTypes(['Browser'], 'browser')
    AddSummariesForProcessTypes(['Renderer'], 'renderer')
    AddSummariesForProcessTypes(['Gpu'], 'gpu')
    AddSummariesForProcessTypes(['Browser', 'Renderer', 'Gpu'], 'total')

    results.AddSummary('commit_charge', 'kb',
                       memory['SystemCommitCharge'] - self._start_commit_charge,
                       data_type='unimportant')
    results.AddSummary('processes', 'count', memory['ProcessCount'],
                       data_type='unimportant')

