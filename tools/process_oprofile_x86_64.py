#!/usr/bin/python
# Copyright (c) 2011 The Native Client Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

""" Post-process Oprofile logs for x86-64 nexes running under sel_ldr.

Maps event counts in the "anon" region, to the appropriate addresses
in the nexe assembly. "Anon" represents the untrusted sandbox.

This will become unnecessary once we get immutable files for our .nexe
so that sel_ldr can use mmap the .nexe instead of copying it in
(Oprofile should understand mmap).

Remember to look at the oprofile log for the time spent in the
trusted code / OS (this only lists time spent in the untrusted code).

"""

# TODO(jvoung): consider using addr2line to look up functions with
# the linenum / file info instead of the using the rangemap.
# Pro: less custom code and possibility of understanding Dwarf info.
# Con: lots of exec()s to cover all the samples...


import commands
import getopt
import math
import re
import sys

def Debug(mesg):
  sys.stdout.flush()             # Make stdout/stderr come out in order.
  print >>sys.stderr, "# ", mesg
  return

def DemangleFunc(fun_name):
  # In case the disassembly was done without the objdump "-C" flag.
  # Heuristic for detecting already demangled names
  # (c++filt will hate you for giving it an already demangled name)
  if ('(' in fun_name or
      '*' in fun_name or
      ':' in fun_name or
      '&' in fun_name):
    return fun_name
  return commands.getoutput("c++filt " + fun_name)

# Assume addresses in inputs (logs and assembly files) are all this base.
ADDRESS_BASE = 16
ADDRESS_DIGIT = '[a-f0-9]'

#--------------- Parse Oprofile Log ---------------

def CheckIfInSelLdrRegion(line, cur_sel_ldr_offset):
  """ Checks if we are reading the part of the oprofile --details log
      pertaining to the untrusted sandbox in sel_ldr's address space.
      Returns the offset of that memory region or None. """
  fields = line.split()
  # cur_sel_ldr_offset should be set if we are already parsing the
  # untrusted sandbox section of the log.
  if cur_sel_ldr_offset:
    # Check if we are exiting the untrusted sandbox section of the log.
    # The header of a new non-untrusted-sandbox section should look like:
    # 00000000 samples  pct  foo.so  foo.so /path-to/foo.so
    if len(fields) >= 6:
      Debug('Likely exiting sel_ldr section to a new section: %s' % fields[3])
      return None
    else:
      return cur_sel_ldr_offset
  else:
    # Check if we are entering the untrusted-sandbox section of the log.
    # The header of such a section should look like:
    #
    # 00000000 samples pct anon (tgid:22067 range:0x7fa5000OFFSET-0x7fa5000END)
    #   sel_ldr  anon (tgid:22067 range:...)
    #
    # I.e., 10 fields...
    if len(fields) == 10 and fields[3] == 'anon' and fields[6] == 'sel_ldr':
      Debug('Likely starting sel_ldr section: %s %s' % (fields[3], fields[6]))
      range_token = fields[9]
      range_re = re.compile('range:0x7' + ADDRESS_DIGIT + '{3}(\d+)-0x7')
      match = range_re.search(range_token)
      if match:
        off_str = match.group(1)
        off = int(off_str, ADDRESS_BASE)
        Debug('Likely offset is %s' % hex(off))
        return off
      else:
        Debug("Couldn't parse offset for: " + str(fields))
        return None
    else:
      return None


def UpdateAddrEventMap(line, sel_ldr_offset, addr_to_event):
  """ Add an event count to the addr_to_event map if the line of data looks
      like an event count. Example:

      vma      samples  %
      0000028a 1        1.8e-04

  """
  fields = line.split()
  if len(fields) == 3:
    address = sel_ldr_offset + int(fields[0], ADDRESS_BASE)
    sample_count = int(fields[1])
    cur = addr_to_event.get(address, 0)
    addr_to_event[address] = cur + sample_count
  return


def GetAddressToEventSelLdr(fd):
  """ Returns a map: address (int) -> event count (int) """
  addr_to_event = {}
  sel_ldr_offset = None
  for line in fd:
    sel_ldr_offset = CheckIfInSelLdrRegion(line, sel_ldr_offset)
    if sel_ldr_offset:
      # If we've parsed the header of the region and know the untrusted
      # sandbox's offset, start picking up event counts.
      UpdateAddrEventMap(line, sel_ldr_offset, addr_to_event)
  fd.seek(0) # Reset for future use...
  return addr_to_event

#--------------- Parse Assembly File ---------------

def CompareBounds((lb1, ub1), (lb2, ub2)):
  # Shouldn't be overlapping, so both the upper and lower
  # should be less than the other's lower bound
  if (lb1 < lb2) and (ub1 < lb2):
    return -1
  elif (lb1 > ub2) and (ub1 > ub2):
    return 1
  else:
    # Somewhere between, not necessarily equal.
    return 0

class RangeMapSorted(object):
  """ Simple range map using a sorted list of pairs
      ((lowerBound, upperBound), data). """
  ranges = []

  # Error indexes (< 0)
  kGREATER = -2
  kLESS = -1

  def FindIndex(self, lb, ub):
    length = len(self.ranges)
    return self.FindIndexFrom(lb, ub,
                              int(math.ceil(length / 2.0)), 0, length)

  def FindIndexFrom(self, lb, ub, CurGuess, CurL, CurH):
    length = len(self.ranges)
    # If it is greater than the last index, it is greater than all.
    if CurGuess >= length:
      return self.kGREATER
    ((lb2, ub2), _) = self.ranges[CurGuess]
    comp = CompareBounds((lb, ub), (lb2, ub2))
    if comp == 0:
      return CurGuess
    elif comp < 0:
      # If it is less than index 0, it is less than all.
      if CurGuess == 0:
        return self.kLESS
      NextL = CurL
      NextH = CurGuess
      NextGuess = CurGuess - int (math.ceil((NextH - NextL) / 2.0))
    else:
      # If it is greater than the last index, it is greater than all.
      if CurGuess >= length - 1:
        return self.kGREATER
      NextL = CurGuess
      NextH = CurH
      NextGuess = CurGuess + int (math.ceil((NextH - NextL) / 2.0))
    return self.FindIndexFrom(lb, ub, NextGuess, NextL, NextH)

  def Add(self, lb, ub, data):
    """ Add a mapping from [lb, ub] --> data """
    index = self.FindIndex(lb, ub)
    range_data = ((lb, ub), data)
    if index == self.kLESS:
      self.ranges.insert(0, range_data)
    elif index == self.kGREATER:
      self.ranges.append(range_data)
    else:
      self.ranges.insert(index, range_data)

  def Lookup(self, key):
    """ Get the data that falls within the range. """
    index = self.FindIndex(key, key)
    # Check if it is out of range.
    if index < 0:
      return None
    ((lb, ub), d) = self.ranges[index]
    # Double check that the key actually falls in range.
    if lb <= key and key <= ub:
      return d
    else:
      return None

  def GetRangeFromKey(self, key):
    index = self.FindIndex(key, key)
    # Check if it is out of range.
    if index < 0:
      return None
    ((lb, ub), _) = self.ranges[index]
    # Double check that the key actually falls in range.
    if lb <= key and key <= ub:
      return (lb, ub)
    else:
      return None

ADDRESS_RE = re.compile('(' + ADDRESS_DIGIT + '+):')
FUNC_RE = re.compile('(' + ADDRESS_DIGIT + '+) <(.*)>:')

def GetAssemblyAddress(line):
  """ Look for lines of assembly that look like

       address: [byte] [byte]...  [instruction in text]
  """
  fields = line.split()
  if len(fields) > 1:
    match = ADDRESS_RE.search(fields[0])
    if match:
      return int(match.group(1), ADDRESS_BASE)
  return None

def GetAssemblyRanges(fd):
  """ Return a RangeMap that tracks the boundaries of each function.
      E.g., [0x20000, 0x2003f] --> "foo"
            [0x20040, 0x20060] --> "bar"
  """
  rmap = RangeMapSorted()
  cur_start = None
  cur_func = None
  cur_end = None
  for line in fd:
    # If we are within a function body...
    if cur_func:
      # Check if it has ended (with a newline)
      if line.strip() == '':
        assert (cur_start and cur_end)
        rmap.Add(cur_start, cur_end, cur_func)
        cur_start = None
        cur_end = None
        cur_func = None
      else:
        maybe_addr = GetAssemblyAddress(line)
        if maybe_addr:
          cur_end = maybe_addr
    else:
      # Not yet within a function body. Check if we are entering.
      # The header should look like:
      # 0000000000020040 <foo>:
      match = FUNC_RE.search(line)
      if match:
        cur_start = int(match.group(1), ADDRESS_BASE)
        cur_func = match.group(2)
  fd.seek(0) # reset for future use.
  return rmap

#--------------- Summarize Data ---------------

def PrintTopFunctions(assembly_ranges, address_to_events):
  """ Prints the N functions with the top event counts """
  func_events = {}
  some_addrs_not_found = False
  for (addr, count) in address_to_events.iteritems():
    func = assembly_ranges.Lookup(addr)
    if (func):
      # Function labels are mostly unique, except when we have ASM labels
      # that we mistake for functions. E.g., "loop:" is a common ASM label.
      # Thus, to get a unique value, we must append the unique key range
      # to the function label.
      (lb, ub) = assembly_ranges.GetRangeFromKey(addr)
      key = (func, lb, ub)
      cur_count = func_events.get(key, 0)
      func_events[key] = cur_count + count
    else:
      Debug('No matching function for addr/count: %s %d'
            % (hex(addr), count))
      some_addrs_not_found = True
  if some_addrs_not_found:
    # Addresses < 0x20000 are likely trampoline addresses.
    Debug('NOTE: sample addrs < 0x20000 are likely trampolines')
  flattened = func_events.items()
  def CompareCounts ((k1, c1), (k2, c2)):
    if c1 < c2:
      return -1
    elif c1 == c2:
      return 0
    else:
      return 1
  flattened.sort(cmp=CompareCounts, reverse=True)
  top_30 = flattened[:30]
  print "============= Top 30 Untrusted Functions ==============="
  print "EVENTS\t\tFUNC [LOW_VMA, UPPER_VMA]"
  for ((func, lb, ub), count) in top_30:
    print "%d\t\t%s [%s, %s]" % (count, DemangleFunc(func), hex(lb), hex(ub))
  print """ TODO: process the rest of the oprofile log and also include
 the top trusted funcs / OS funcs. For now you must look at the log
 yourself for that data. It may end up being a large portion of
 the execution time."""


#--------------- Annotate Assembly ---------------

def PrintAnnotatedAssembly(fd_in, address_to_events, fd_out):
  """ Writes to output, a version of assembly_file which has event
      counts in the form #; EVENTS: N
      This lets us know which instructions took the most time, etc.
  """
  for line in fd_in:
    line = line.strip()
    maybe_addr = GetAssemblyAddress(line)
    if maybe_addr in address_to_events:
      event_count = address_to_events[maybe_addr]
      print >>fd_out, "%s    #; EVENTS: %d" % (line, event_count)
    else:
      print >>fd_out, line
  fd_in.seek(0) # reset for future use.

#--------------- Main ---------------

def main(argv):
  try:
    opts, args = getopt.getopt(argv[1:],
                               'l:s:o:',
                               ['oprofilelog=',
                                'assembly=',
                                'output=',
                                ])
    assembly_file = None
    assembly_fd = None
    oprof_log = None
    oprof_fd = None
    output = sys.stdout
    out_name = None
    for o, a in opts:
      if o in ('-l', '--oprofilelog'):
        oprof_log = a
        oprof_fd = open(oprof_log, 'r')
      elif o in ('-s', '--assembly'):
        assembly_file = a
        assembly_fd = open(assembly_file, 'r')
      elif o in ('-o', '--output'):
        out_name = a
        output = open(out_name, 'w')
      else:
        assert False, 'unhandled option'
    if assembly_file and oprof_log:
      Debug('Parsing assembly file of nexe: %s' % assembly_file)
      assembly_ranges = GetAssemblyRanges(assembly_fd)
      Debug('Parsing oprofile log: %s' % oprof_log)
      address_to_events = GetAddressToEventSelLdr(oprof_fd)
      Debug('Printing the top functions (most events)')
      PrintTopFunctions(assembly_ranges, address_to_events)
      Debug('Printing annotated assembly to %s (or stdout)' % out_name)
      PrintAnnotatedAssembly(assembly_fd, address_to_events, output)
    else:
      print 'Need assembly file(%s) and oprofile log(%s)!' \
          % (assembly_file, oprof_log)
      sys.exit(2)
  except getopt.GetoptError, err:
    print str(err)
    sys.exit(2)

if __name__ == '__main__':
  main(sys.argv)
