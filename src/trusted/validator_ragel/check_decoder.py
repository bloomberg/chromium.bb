#!/usr/bin/python
# Copyright (c) 2012 The Native Client Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""
Generate all acceptable sequences from acyclic DFA, run objdump and
decoder on them and compare results.
"""

import itertools
import multiprocessing
import optparse
import os
import re
import subprocess
import tempfile

import dfa_parser


def GetNumSuffixes(start_state):
  """Compute number of minimal suffixes automaton accepts from each state.

  For each state reachable from given state, compute number of paths in the
  automaton that start from that state, end in the accepting state and do not
  pass through accepting states in between.

  It is assumed that there are no cyclic paths going entirely through
  non-accepting states.

  Args:
    start_state: start state.

  Returns:
    Dictionary from reachable states to numbers of suffixes.
  """
  num_suffixes = {}

  def ComputeNumSuffixes(state):
    if state in num_suffixes:
      return

    if state.is_accepting:
      num_suffixes[state] = 1

      # Even though the state itself is accepting, there may be more reachable
      # states behind it.
      for t in state.forward_transitions.values():
        ComputeNumSuffixes(t.to_state)

      return

    if state.any_byte:
      next_state = state.forward_transitions[0].to_state
      ComputeNumSuffixes(next_state)
      num_suffixes[state] = num_suffixes[next_state]
      return

    count = 0
    for t in state.forward_transitions.values():
      ComputeNumSuffixes(t.to_state)
      count += num_suffixes[t.to_state]
    num_suffixes[state] = count

  ComputeNumSuffixes(start_state)
  return num_suffixes


def TraverseTree(state, final_callback, prefix, anyfield=0x01):
  if state.is_accepting:
    final_callback(prefix)
    return

  if state.any_byte:
    assert anyfield < 256
    TraverseTree(
        state.forward_transitions[0].to_state,
        final_callback,
        prefix + [anyfield],
        anyfield=anyfield + 0x11)
    # We add 0x11 each time to get sequence 01 12 23 etc.

    # TODO(shcherbina): change it to much nicer 0x22 once
    # http://code.google.com/p/nativeclient/issues/detail?id=3164 is fixed
    # (right now we want to keep immediates small to avoid problem with sign
    # extension).
  else:
    for byte, t in state.forward_transitions.iteritems():
      TraverseTree(
          t.to_state,
          final_callback,
          prefix + [byte])


def RoughlyEqual(s1, s2):
  """Check whether lines are equal up to whitespaces."""
  return s1.split() == s2.split()


class WorkerState(object):
  __slots__ = [
      'total_instructions',
      '_num_instructions',
      '_asm_file',
      '_file_prefix',
  ]

  def __init__(self, prefix):
    self._file_prefix = 'check_decoder_%s_' % '_'.join(map(hex, prefix))
    self.total_instructions = 0

    self._StartNewFile()

  def _StartNewFile(self):
    self._asm_file = tempfile.NamedTemporaryFile(
        mode='wt',
        prefix=self._file_prefix,
        suffix='.s',
        delete=False)
    self._asm_file.write('.text\n')
    self._num_instructions = 0

  def ReceiveInstruction(self, bytes):
    # We do not disassemble x87 instructions prefixed with fwait as objdump
    # does. To avoid such situations in enumeration test, we insert nop after
    # fwait.
    if (bytes == [0x9b] or
        len(bytes) == 2 and bytes[1] == 0x9b and 0x40 <= bytes[0] < 0x4f):
      bytes = bytes + [0x90]
    self._asm_file.write('  .byte %s\n' % ','.join(map(hex, bytes)))
    self.total_instructions += 1
    self._num_instructions += 1
    if self._num_instructions == 1000000:
      self.Finish()
      self._StartNewFile()

  def _CheckFile(self):
    try:
      object_file = tempfile.NamedTemporaryFile(
          prefix=self._file_prefix,
          suffix='.o',
          delete=False)
      object_file.close()
      subprocess.check_call([
          options.gas,
          '--%s' % options.bits,
          self._asm_file.name,
          '-o', object_file.name])

      objdump_proc = subprocess.Popen(
          [options.objdump, '-d', object_file.name],
          stdout=subprocess.PIPE)

      decoder_proc = subprocess.Popen(
          [options.decoder, object_file.name],
          stdout=subprocess.PIPE)

      # Objdump prints few lines about file and section before disassembly
      # listing starts, so we have to skip that.
      objdump_header_size = 7

      for line1, line2 in itertools.izip_longest(
          itertools.islice(objdump_proc.stdout, objdump_header_size, None),
          decoder_proc.stdout,
          fillvalue=None):

        if not RoughlyEqual(line1, line2):
          print 'objdump: %r' % line1
          print 'decoder: %r' % line2
          raise AssertionError('%r != %r' % (line1, line2))

      return_code = objdump_proc.wait()
      assert return_code == 0

      return_code = decoder_proc.wait()
      assert return_code == 0

    finally:
      os.remove(self._asm_file.name)
      os.remove(object_file.name)

  def Finish(self):
    self._asm_file.close()
    self._CheckFile()


def Worker((prefix, state_index)):
  worker_state = WorkerState(prefix)

  try:
    TraverseTree(states[state_index],
                 final_callback=worker_state.ReceiveInstruction,
                 prefix=prefix)
  finally:
    worker_state.Finish()

  return prefix, worker_state.total_instructions


def ParseOptions():
  parser = optparse.OptionParser(
      usage='%prog [options] xmlfile',
      description=__doc__)
  parser.add_option('--bits',
                    type=int,
                    help='The subarchitecture: 32 or 64')
  parser.add_option('--gas',
                    help='Path to GNU AS executable')
  parser.add_option('--objdump',
                    help='Path to objdump executable')
  parser.add_option('--decoder',
                    help='Path to decoder executable')
  options, args = parser.parse_args()
  if options.bits not in [32, 64]:
    parser.error('specify --bits 32 or --bits 64')
  if not (options.gas and options.objdump and options.decoder):
    parser.error('specify path to gas, objdump and decoder')

  if len(args) != 1:
    parser.error('specify one xml file')

  (xml_file,) = args

  return options, xml_file


# We are doing it here, not inside main, because we need options in subprocesses
# spawned by multiprocess.
options, xml_file = ParseOptions()
# And we don't want states graph to be passed again for each task - it's too
# slow for some reason.
states, initial_state = dfa_parser.ParseXml(xml_file)


def main():
  # Decoder is supposed to have one accepting state.
  accepting_states = [state for state in states if state.is_accepting]
  assert accepting_states == [initial_state]

  assert not initial_state.any_byte

  num_suffixes = GetNumSuffixes(initial_state)
  # We can't just write 'num_suffixes[initial_state]' because
  # initial state is accepting.
  total_instructions = sum(num_suffixes[t.to_state]
                           for t in initial_state.forward_transitions.values())
  print total_instructions, 'instructions total'

  # We parallelize by first two bytes.
  tasks = []
  for byte1, t1 in sorted(initial_state.forward_transitions.items()):
    state1 = t1.to_state
    if state1.any_byte or state1.is_accepting or num_suffixes[state1] < 10**7:
      tasks.append(([byte1], states.index(state1)))
      continue
    for byte2, t2 in sorted(state1.forward_transitions.items()):
      state2 = t2.to_state
      tasks.append(([byte1, byte2], states.index(state2)))

  print len(tasks), 'tasks'

  pool = multiprocessing.Pool()

  results = pool.imap(Worker, tasks)

  total = 0
  for prefix, count in results:
    print ', '.join(map(hex, prefix))
    total += count

  print total, 'instructions were processed'


if __name__ == '__main__':
  main()
