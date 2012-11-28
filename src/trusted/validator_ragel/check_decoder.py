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
import subprocess
import tempfile

from lxml import etree


ANYFIELD_BEGIN_ACTIONS = [
    'disp8_operand_begin', 'disp32_operand_begin', 'disp64_operand_begin',
    'rel8_operand_begin', 'rel16_operand_begin', 'rel32_operand_begin',
    'imm8_operand_begin', 'imm16_operand_begin', 'imm32_operand_begin',
    'imm64_operand_begin'
]

ANYFIELD_END_ACTIONS = [
    'disp8_operand_end', 'disp32_operand_end', 'disp64_operand_end',
    'rel8_operand_end', 'rel16_operand_end', 'rel32_operand_end',
    'imm8_operand_end', 'imm16_operand_end', 'imm32_operand_end',
    'imm64_operand_end'
]

ANY_ACTIONS = set(ANYFIELD_BEGIN_ACTIONS + ANYFIELD_END_ACTIONS)


def IncludesActionFrom(dfa_tree, action_table_id, action_list):
  """Returns true if action_table_id includes action from action_list."""
  if action_table_id == 'x':
    return False
  (action_table,) = dfa_tree.xpath('//action_table[@id="%s"]' % action_table_id)
  actions = action_table.text.split(' ')
  for action_id in actions:
    (action,) = dfa_tree.xpath('//action[@id="%s"]' % action_id)
    action_name = action.get('name')
    assert action_name is not None, 'name of action "%s" not found' % action_id
    if action_name in action_list:
      return True
  return False


class DfaState(object):
  __slots__ = [
      'id',
      'transitions',
      'is_final',
      'any_byte',
      '_num_suffixes',
  ]

  def __init__(self):
    self._num_suffixes = None

  @property
  def num_suffixes(self):
    if self._num_suffixes is not None:
      return self._num_suffixes
    else:
      count = 0
      if self.is_final:
        count += 1
      if self.any_byte:
        count += self.transitions[0].num_suffixes
      else:
        count += sum(next_state.num_suffixes
                   for next_state in self.transitions.values())
      self._num_suffixes = count
      return count


def ReadStates(dfa_tree):
  states = []

  xml_states = dfa_tree.xpath('//state')

  for xml_state in xml_states:
    # Ragel always dumps states in order, but we better check just in case.
    state_id = int(xml_state.get('id'))
    assert state_id == len(states)

    state = DfaState()
    state.id = state_id
    state.transitions = {}
    state.is_final = bool(xml_state.get('final'))
    state.any_byte = False

    (xml_transitions,) = xml_state.xpath('trans_list')
    for t in xml_transitions.getchildren():
      range_begin, range_end, next_state, action_table_id = t.text.split(' ')
      range_begin = int(range_begin)
      range_end = int(range_end) + 1
      next_state = int(next_state)

      for byte in xrange(range_begin, range_end):
        state.transitions[byte] = next_state

      if range_begin == 0 and range_end == 256:
        # We check if transition is marked with any of the anyactions.
        # It only makes sense to do when all transitions are the same
        # (that is, when they all are covered by a single range [0, 256)).
        assert len(xml_transitions.getchildren()) == 1

        if IncludesActionFrom(dfa_tree, action_table_id, ANY_ACTIONS):
          state.any_byte = True

        state_actions = xml_state.xpath('state_actions')
        if state_actions:
          (state_actions,) = state_actions
          to_action, from_action, eof_action = state_actions.text.split()
          assert from_action == eof_action == 'x'
          if IncludesActionFrom(dfa_tree, to_action, any_actions):
            state.any_byte = True
        break

    states.append(state)

  for state in states:
    for byte, next_state in state.transitions.items():
      state.transitions[byte] = states[next_state]

  return states


def TraverseTree(state, final_callback, prefix, anyfield=0x01):
  transitions = state.transitions

  if state.is_final:
    final_callback(prefix)

  if state.any_byte:
    assert anyfield < 256
    TraverseTree(
        transitions[0],
        final_callback,
        prefix + ',' + hex(anyfield),
        anyfield=anyfield + 0x11)
    # We add 0x11 each time to get sequence 01 12 23 etc.

    # TODO(shcherbina): change it to much nicer 0x22 once
    # http://code.google.com/p/nativeclient/issues/detail?id=3164 is fixed
    # (right now we want to keep immediates small to avoid problem with sign
    # extension).
  else:
    for byte, next_state in transitions.iteritems():
      TraverseTree(
          next_state,
          final_callback,
          prefix + ',' + hex(byte))


class WorkerState(object):
  __slots__ = [
      'total_instructions',
      '_num_instructions',
      '_asm_file',
      '_file_prefix',
  ]

  def __init__(self, prefix):
    self._file_prefix = 'check_decoder_%s_' % prefix.replace(',', '_')
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

  def ReceiveInstruction(self, s):
    self._asm_file.write('  .byte ' + s + '\n')
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

        # TODO(shcherbina): remove this exception once
        # http://code.google.com/p/nativeclient/issues/detail?id=3165 is fixed.
        if 'extrq' in line1 or 'insertq' in line1:
          continue

        if line1 != line2:
          print 'objdump: %r' % line1
          print 'decoder: %r' % line2
          raise AssertionError("%r != %r" % (line1, line2))

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


def Worker((prefix, state)):
  worker_state = WorkerState(prefix)

  try:
    TraverseTree(state,
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


def main():
  dfa_tree = etree.parse(xml_file)

  entries = dfa_tree.xpath('//entry')
  assert len(entries) == 1, 'multiple entry points'
  entry = int(entries[0].text)

  states = ReadStates(dfa_tree)

  initial_state = states[entry]

  assert not initial_state.is_final
  assert not initial_state.any_byte

  # We parallelize by first two bytes.
  tasks = []
  for byte1, state1 in sorted(initial_state.transitions.items()):
    if state1.any_byte or state1.is_final or state1.num_suffixes < 10**7:
      tasks.append((hex(byte1), state1))
      continue
    for byte2, state2 in sorted(state1.transitions.items()):
      tasks.append((hex(byte1) + ',' + hex(byte2), state2))

  print len(tasks), 'tasks'

  pool = multiprocessing.Pool()

  results = pool.imap(Worker, tasks)

  total = 0
  for prefix, count in results:
    print prefix
    total += count

  print total, 'instructions were processed'


if __name__ == '__main__':
  main()
