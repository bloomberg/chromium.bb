# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import json
import logging

from metrics import Metric
from telemetry.value import scalar


_COUNTER_NAMES = [
    'V8.OsMemoryAllocated',
    'V8.MemoryNewSpaceBytesAvailable',
    'V8.MemoryNewSpaceBytesCommitted',
    'V8.MemoryNewSpaceBytesUsed',
    'V8.MemoryOldPointerSpaceBytesAvailable',
    'V8.MemoryOldPointerSpaceBytesCommitted',
    'V8.MemoryOldPointerSpaceBytesUsed',
    'V8.MemoryOldDataSpaceBytesAvailable',
    'V8.MemoryOldDataSpaceBytesCommitted',
    'V8.MemoryOldDataSpaceBytesUsed',
    'V8.MemoryCodeSpaceBytesAvailable',
    'V8.MemoryCodeSpaceBytesCommitted',
    'V8.MemoryCodeSpaceBytesUsed',
    'V8.MemoryMapSpaceBytesAvailable',
    'V8.MemoryMapSpaceBytesCommitted',
    'V8.MemoryMapSpaceBytesUsed',
    'V8.MemoryCellSpaceBytesAvailable',
    'V8.MemoryCellSpaceBytesCommitted',
    'V8.MemoryCellSpaceBytesUsed',
    'V8.MemoryPropertyCellSpaceBytesAvailable',
    'V8.MemoryPropertyCellSpaceBytesCommitted',
    'V8.MemoryPropertyCellSpaceBytesUsed',
    'V8.MemoryLoSpaceBytesAvailable',
    'V8.MemoryLoSpaceBytesCommitted',
    'V8.MemoryLoSpaceBytesUsed',
    'V8.SizeOf_ACCESSOR_PAIR_TYPE',
    'V8.SizeOf_ACCESS_CHECK_INFO_TYPE',
    'V8.SizeOf_ALIASED_ARGUMENTS_ENTRY_TYPE',
    'V8.SizeOf_ALLOCATION_MEMENTO_TYPE',
    'V8.SizeOf_ALLOCATION_SITE_TYPE',
    'V8.SizeOf_ASCII_INTERNALIZED_STRING_TYPE',
    'V8.SizeOf_ASCII_STRING_TYPE',
    'V8.SizeOf_BOX_TYPE',
    'V8.SizeOf_BREAK_POINT_INFO_TYPE',
    'V8.SizeOf_BYTE_ARRAY_TYPE',
    'V8.SizeOf_CALL_HANDLER_INFO_TYPE',
    'V8.SizeOf_CELL_TYPE',
    'V8.SizeOf_CODE_AGE-NotExecuted',
    'V8.SizeOf_CODE_AGE-ExecutedOnce',
    'V8.SizeOf_CODE_AGE-NoAge',
    'V8.SizeOf_CODE_AGE-Quadragenarian',
    'V8.SizeOf_CODE_AGE-Quinquagenarian',
    'V8.SizeOf_CODE_AGE-Sexagenarian',
    'V8.SizeOf_CODE_AGE-Septuagenarian',
    'V8.SizeOf_CODE_AGE-Octogenarian',
    'V8.SizeOf_CODE_CACHE_TYPE',
    'V8.SizeOf_CODE_TYPE',
    'V8.SizeOf_CODE_TYPE-BINARY_OP_IC',
    'V8.SizeOf_CODE_TYPE-BUILTIN',
    'V8.SizeOf_CODE_TYPE-CALL_IC',
    'V8.SizeOf_CODE_TYPE-COMPARE_IC',
    'V8.SizeOf_CODE_TYPE-COMPARE_NIL_IC',
    'V8.SizeOf_CODE_TYPE-FUNCTION',
    'V8.SizeOf_CODE_TYPE-KEYED_CALL_IC',
    'V8.SizeOf_CODE_TYPE-KEYED_LOAD_IC',
    'V8.SizeOf_CODE_TYPE-KEYED_STORE_IC',
    'V8.SizeOf_CODE_TYPE-LOAD_IC',
    'V8.SizeOf_CODE_TYPE-OPTIMIZED_FUNCTION',
    'V8.SizeOf_CODE_TYPE-REGEXP',
    'V8.SizeOf_CODE_TYPE-STORE_IC',
    'V8.SizeOf_CODE_TYPE-STUB',
    'V8.SizeOf_CODE_TYPE-TO_BOOLEAN_IC',
    'V8.SizeOf_CONS_ASCII_INTERNALIZED_STRING_TYPE',
    'V8.SizeOf_CONS_ASCII_STRING_TYPE',
    'V8.SizeOf_CONS_INTERNALIZED_STRING_TYPE',
    'V8.SizeOf_CONS_STRING_TYPE',
    'V8.SizeOf_DEBUG_INFO_TYPE',
    'V8.SizeOf_DECLARED_ACCESSOR_DESCRIPTOR_TYPE',
    'V8.SizeOf_DECLARED_ACCESSOR_INFO_TYPE',
    'V8.SizeOf_EXECUTABLE_ACCESSOR_INFO_TYPE',
    'V8.SizeOf_EXTERNAL_ASCII_INTERNALIZED_STRING_TYPE',
    'V8.SizeOf_EXTERNAL_ASCII_STRING_TYPE',
    'V8.SizeOf_EXTERNAL_BYTE_ARRAY_TYPE',
    'V8.SizeOf_EXTERNAL_DOUBLE_ARRAY_TYPE',
    'V8.SizeOf_EXTERNAL_FLOAT_ARRAY_TYPE',
    'V8.SizeOf_EXTERNAL_INTERNALIZED_STRING_TYPE',
    'V8.SizeOf_EXTERNAL_INTERNALIZED_STRING_WITH_ONE_BYTE_DATA_TYPE',
    'V8.SizeOf_EXTERNAL_INT_ARRAY_TYPE',
    'V8.SizeOf_EXTERNAL_PIXEL_ARRAY_TYPE',
    'V8.SizeOf_EXTERNAL_SHORT_ARRAY_TYPE',
    'V8.SizeOf_EXTERNAL_STRING_TYPE',
    'V8.SizeOf_EXTERNAL_STRING_WITH_ONE_BYTE_DATA_TYPE',
    'V8.SizeOf_EXTERNAL_UNSIGNED_BYTE_ARRAY_TYPE',
    'V8.SizeOf_EXTERNAL_UNSIGNED_INT_ARRAY_TYPE',
    'V8.SizeOf_EXTERNAL_UNSIGNED_SHORT_ARRAY_TYPE',
    'V8.SizeOf_FILLER_TYPE',
    'V8.SizeOf_FIXED_ARRAY-DESCRIPTOR_ARRAY_SUB_TYPE',
    'V8.SizeOf_FIXED_ARRAY-DICTIONARY_ELEMENTS_SUB_TYPE',
    'V8.SizeOf_FIXED_ARRAY-DICTIONARY_PROPERTIES_SUB_TYPE',
    'V8.SizeOf_FIXED_ARRAY-FAST_ELEMENTS_SUB_TYPE',
    'V8.SizeOf_FIXED_ARRAY-FAST_PROPERTIES_SUB_TYPE',
    'V8.SizeOf_FIXED_ARRAY-MAP_CODE_CACHE_SUB_TYPE',
    'V8.SizeOf_FIXED_ARRAY-SCOPE_INFO_SUB_TYPE',
    'V8.SizeOf_FIXED_ARRAY-STRING_TABLE_SUB_TYPE',
    'V8.SizeOf_FIXED_ARRAY-TRANSITION_ARRAY_SUB_TYPE',
    'V8.SizeOf_FIXED_ARRAY_TYPE',
    'V8.SizeOf_FIXED_DOUBLE_ARRAY_TYPE',
    'V8.SizeOf_FOREIGN_TYPE',
    'V8.SizeOf_FREE_SPACE_TYPE',
    'V8.SizeOf_FUNCTION_TEMPLATE_INFO_TYPE',
    'V8.SizeOf_HEAP_NUMBER_TYPE',
    'V8.SizeOf_INTERCEPTOR_INFO_TYPE',
    'V8.SizeOf_INTERNALIZED_STRING_TYPE',
    'V8.SizeOf_JS_ARRAY_BUFFER_TYPE',
    'V8.SizeOf_JS_ARRAY_TYPE',
    'V8.SizeOf_JS_BUILTINS_OBJECT_TYPE',
    'V8.SizeOf_JS_CONTEXT_EXTENSION_OBJECT_TYPE',
    'V8.SizeOf_JS_DATA_VIEW_TYPE',
    'V8.SizeOf_JS_DATE_TYPE',
    'V8.SizeOf_JS_FUNCTION_PROXY_TYPE',
    'V8.SizeOf_JS_FUNCTION_TYPE',
    'V8.SizeOf_JS_GENERATOR_OBJECT_TYPE',
    'V8.SizeOf_JS_GLOBAL_OBJECT_TYPE',
    'V8.SizeOf_JS_GLOBAL_PROXY_TYPE',
    'V8.SizeOf_JS_MAP_TYPE',
    'V8.SizeOf_JS_MESSAGE_OBJECT_TYPE',
    'V8.SizeOf_JS_MODULE_TYPE',
    'V8.SizeOf_JS_OBJECT_TYPE',
    'V8.SizeOf_JS_PROXY_TYPE',
    'V8.SizeOf_JS_REGEXP_TYPE',
    'V8.SizeOf_JS_SET_TYPE',
    'V8.SizeOf_JS_TYPED_ARRAY_TYPE',
    'V8.SizeOf_JS_VALUE_TYPE',
    'V8.SizeOf_JS_WEAK_MAP_TYPE',
    'V8.SizeOf_JS_WEAK_SET_TYPE',
    'V8.SizeOf_MAP_TYPE',
    'V8.SizeOf_OBJECT_TEMPLATE_INFO_TYPE',
    'V8.SizeOf_ODDBALL_TYPE',
    'V8.SizeOf_POLYMORPHIC_CODE_CACHE_TYPE',
    'V8.SizeOf_PROPERTY_CELL_TYPE',
    'V8.SizeOf_SCRIPT_TYPE',
    'V8.SizeOf_SHARED_FUNCTION_INFO_TYPE',
    'V8.SizeOf_SHORT_EXTERNAL_ASCII_INTERNALIZED_STRING_TYPE',
    'V8.SizeOf_SHORT_EXTERNAL_ASCII_STRING_TYPE',
    'V8.SizeOf_SHORT_EXTERNAL_INTERNALIZED_STRING_TYPE',
    'V8.SizeOf_SHORT_EXTERNAL_INTERNALIZED_STRING_WITH_ONE_BYTE_DATA_TYPE',
    'V8.SizeOf_SHORT_EXTERNAL_STRING_TYPE',
    'V8.SizeOf_SHORT_EXTERNAL_STRING_WITH_ONE_BYTE_DATA_TYPE',
    'V8.SizeOf_SIGNATURE_INFO_TYPE',
    'V8.SizeOf_SLICED_ASCII_STRING_TYPE',
    'V8.SizeOf_SLICED_STRING_TYPE',
    'V8.SizeOf_STRING_TYPE',
    'V8.SizeOf_SYMBOL_TYPE',
    'V8.SizeOf_TYPE_FEEDBACK_INFO_TYPE',
    'V8.SizeOf_TYPE_SWITCH_INFO_TYPE'
  ]

class V8ObjectStatsMetric(Metric):
  """V8ObjectStatsMetric gathers statistics on the size of types in the V8 heap.

  It does this by enabling the --track_gc_object_stats flag on V8 and reading
  these statistics from the StatsTableMetric.
  """

  def __init__(self, counters=None):
    super(V8ObjectStatsMetric, self).__init__()
    self._results = None
    self._counters = counters or _COUNTER_NAMES

  @classmethod
  def CustomizeBrowserOptions(cls, options):
    options.AppendExtraBrowserArgs([
        '--enable-stats-table',
        '--enable-benchmarking',
        '--js-flags=--track_gc_object_stats --expose_gc',
        # TODO(rmcilroy): This is needed for --enable-stats-table.  Update once
        # https://codereview.chromium.org/22911027/ lands.
        '--no-sandbox'
    ])

  @staticmethod
  def GetV8StatsTable(tab, counters):

    return tab.EvaluateJavaScript("""
     (function(counters) {
       var results = {};
       if (!window.chrome || !window.chrome.benchmarking)
        return results;
       try {
        window.gc();  // Trigger GC to ensure stats are checkpointed.
       } catch(e) {
        // window.gc() could have been mapped to something else, just continue.
       }
       for (var i = 0; i < counters.length; i++)
         results[counters[i]] =
             chrome.benchmarking.counterForRenderer(counters[i]);
       return results;
     })(%s);
     """ % json.dumps(counters))

  def Start(self, page, tab):
    """Do Nothing."""
    pass

  def Stop(self, page, tab):
    """Get the values in the stats table after the page is loaded."""
    self._results = V8ObjectStatsMetric.GetV8StatsTable(tab, self._counters)
    if not self._results:
      logging.warning('No V8 object stats from website: ' + page.display_name)

  def AddResults(self, tab, results):
    """Add results for this page to the results object."""
    assert self._results != None, 'Must call Stop() first'
    for counter_name in self._results:
      display_name = counter_name.replace('.', '_')
      results.AddValue(scalar.ScalarValue(
          results.current_page, display_name, 'kb',
          self._results[counter_name] / 1024.0))
