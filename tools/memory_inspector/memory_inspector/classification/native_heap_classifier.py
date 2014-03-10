# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""This module classifies NativeHeap objects filtering their allocations.

The only filter currently available is 'stacktrace', which works as follows:

{'name': 'rule-1', 'stacktrace': 'foo' }
{'name': 'rule-2', 'stacktrace': ['foo', r'bar\s+baz']}

rule-1 will match any allocation that has 'foo' in one of its  stack frames.
rule-2 will match any allocation that has a stack frame matching 'foo' AND a
followed by a stack frame matching 'bar baz'. Note that order matters, so rule-2
will not match a stacktrace like ['bar baz', 'foo'].

TODO(primiano): introduce more filters after the first prototype with UI, for
instance, filter by source file path, library file name or by allocation size.
"""

import re

from memory_inspector.classification import results
from memory_inspector.classification import rules
from memory_inspector.core import exceptions
from memory_inspector.core import native_heap


_RESULT_KEYS = ['bytes_allocated']


def LoadRules(content):
  """Loads and parses a native-heap rule tree from a content (string).

  Returns:
    An instance of |rules.Rule|, which nodes are |_NHeapRule| instances.
  """
  return rules.Load(content, _NHeapRule)


def Classify(nativeheap, rule_tree):
  """Create aggregated results of native heaps using the provided rules.

  Args:
    nativeheap: the heap dump being processed (a |NativeHeap| instance).
    rule_tree: the user-defined rules that define the filtering categories.

  Returns:
    An instance of |AggreatedResults|.
  """
  assert(isinstance(nativeheap, native_heap.NativeHeap))
  assert(isinstance(rule_tree, rules.Rule))

  res = results.AggreatedResults(rule_tree, _RESULT_KEYS)
  for allocation in nativeheap.allocations:
    res.AddToMatchingNodes(allocation, [allocation.total_size])
  return res


class _NHeapRule(rules.Rule):
  def __init__(self, name, filters):
    super(_NHeapRule, self).__init__(name)
    stacktrace_regexs = filters.get('stacktrace', [])
    # The 'stacktrace' filter can be either a string (simple case, one regex) or
    # a list of strings (complex case, see doc in the header of this file).
    if isinstance(stacktrace_regexs, basestring):
      stacktrace_regexs = [stacktrace_regexs]
    self._stacktrace_regexs = []
    for regex in stacktrace_regexs:
      try:
        self._stacktrace_regexs.append(re.compile(regex))
      except re.error, descr:
        raise exceptions.MemoryInspectorException(
            'Regex parse error "%s" : %s' % (regex, descr))

  def Match(self, allocation):
    if not self._stacktrace_regexs:
      return True
    cur_regex_idx = 0
    cur_regex = self._stacktrace_regexs[0]
    for frame in allocation.stack_trace.frames:
      if frame.symbol and cur_regex.search(frame.symbol.name):
        # The current regex has been matched.
        if cur_regex_idx == len(self._stacktrace_regexs) - 1:
          return True  # All the provided regexs have been matched, we're happy.
        cur_regex_idx += 1
        cur_regex = self._stacktrace_regexs[cur_regex_idx]

    return False  # Not all the provided regexs have been matched.