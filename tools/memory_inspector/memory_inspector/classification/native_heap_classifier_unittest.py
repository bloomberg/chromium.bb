# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import unittest

from memory_inspector.classification import native_heap_classifier
from memory_inspector.core import native_heap
from memory_inspector.core import stacktrace
from memory_inspector.core import symbol


_TEST_RULES = """
[
{
  'name': 'content',
  'stacktrace': r'content::',
  'children': [
    {
      'name': 'browser',
      'stacktrace': r'content::browser',
    },
    {
      'name': 'renderer',
      'stacktrace': r'content::renderer',
    },
  ],
},
{
  'name': 'ashmem_in_skia',
  'stacktrace': [r'sk::', r'ashmem::'],
},
]
"""

_TEST_STACK_TRACES = [
    (3, ['stack_frame_0::foo()', 'this_goes_under_totals_other']),
    (5, ['foo', 'content::browser::something()', 'bar']),
    (7, ['content::browser::something_else()']),
    (11, ['content::browser::something_else_more()', 'foo']),
    (13, ['foo', 'content::renderer::something()', 'bar']),
    (17, ['content::renderer::something_else()']),
    (19, ['content::renderer::something_else_more()', 'foo']),
    (23, ['content::something_different']),
    (29, ['foo', 'sk::something', 'not_ashsmem_goes_into_totals_other']),
    (31, ['foo', 'sk::something', 'foo::bar', 'sk::foo::ashmem::alloc()']),
    (37, ['foo', 'sk::something', 'sk::foo::ashmem::alloc()']),
    (43, ['foo::ashmem::alloc()', 'sk::foo', 'wrong_order_goes_into_totals'])
]

_EXPECTED_RESULTS = {
    'Total':                         [238],
    'Total::content':                [95],
    'Total::content::browser':       [23],  # 5 + 7 + 11.
    'Total::content::renderer':      [49],  # 13 + 17 + 19.
    'Total::content::content-other': [23],
    'Total::ashmem_in_skia':         [68],  # 31 + 37.
    'Total::Total-other':            [75],  # 3 + 29 + 43.
}


class NativeHeapClassifierTest(unittest.TestCase):
  def runTest(self):
    rule_tree = native_heap_classifier.LoadRules(_TEST_RULES)
    nheap = native_heap.NativeHeap()
    mock_addr = 0
    for test_entry in _TEST_STACK_TRACES:
      mock_strace = stacktrace.Stacktrace()
      for mock_btstr in test_entry[1]:
        mock_addr += 4  # Addr is irrelevant, just keep it distinct.
        mock_frame = stacktrace.Frame(mock_addr)
        mock_frame.SetSymbolInfo(symbol.Symbol(mock_btstr))
        mock_strace.Add(mock_frame)
      nheap.Add(native_heap.Allocation(
          size=test_entry[0], count=1, stack_trace=mock_strace))

    res = native_heap_classifier.Classify(nheap, rule_tree)

    def CheckResult(node, prefix):
      node_name = prefix + node.name
      self.assertIn(node_name, _EXPECTED_RESULTS)
      self.assertEqual(node.values, _EXPECTED_RESULTS[node_name])
      for child in node.children:
        CheckResult(child, node_name + '::')

    CheckResult(res.total, '')