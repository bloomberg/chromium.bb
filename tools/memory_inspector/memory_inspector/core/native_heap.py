# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from memory_inspector.core import stacktrace
from memory_inspector.core import symbol


class NativeHeap(object):
  """A snapshot of outstanding (i.e. not freed) native allocations.

  This is typically obtained by calling |backends.Process|.DumpNativeHeap()
  """

  def __init__(self):
    self.allocations = []  # list of individual |Allocation|s.
    self.stack_frames = {}  # absolute_address (int) -> |stacktrace.Frame|.

  def Add(self, allocation):
    assert(isinstance(allocation, Allocation))
    self.allocations += [allocation]

  def GetStackFrame(self, absolute_addr):
    """Guarantees that multiple calls with the same addr return the same obj."""
    assert(isinstance(absolute_addr, (long, int)))
    stack_frame = self.stack_frames.get(absolute_addr)
    if not stack_frame:
      stack_frame = stacktrace.Frame(absolute_addr)
      self.stack_frames[absolute_addr] = stack_frame
    return stack_frame

  def SymbolizeUsingSymbolDB(self, symbols):
    assert(isinstance(symbols, symbol.Symbols))
    for stack_frame in self.stack_frames.itervalues():
      if stack_frame.exec_file_rel_path is None:
        continue
      sym = symbols.Lookup(stack_frame.exec_file_rel_path, stack_frame.offset)
      if sym:
        stack_frame.SetSymbolInfo(sym)


class Allocation(object):
  """A Native allocation, modeled in a size*count fashion.

  |count| is the number of identical stack_traces which performed the allocation
  of |size| bytes.
  """

  def __init__(self, size, count, stack_trace):
    assert(isinstance(stack_trace, stacktrace.Stacktrace))
    self.size = size  # in bytes.
    self.count = count
    self.stack_trace = stack_trace

  @property
  def total_size(self):
    return self.size * self.count

  def __str__(self):
    return '%d x %d : %s' % (self.count, self.size, self.stack_trace)
