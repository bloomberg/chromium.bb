#!/usr/bin/python
# Copyright (c) 2012 The Native Client Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.


"""Generates C equivalent of the DFA defined in a dot file for the decoder test.

Reads the dot file from stdin and writes the output to stdout.

Some assumptions are made that does not allow parsing arbitrary dot files:
* The digraph definition starts by listing all nodes with a number of shapes:
  * "shape = doublecircle" denotes a final state node
  * all other states are listed under "shape = point" or "shape = circle"
* The initial state is pointed to by the edge going from node named "ENTRY".
* Each edge definition fits in one line.
* Edge label describes a list of bytes, byte ranges (written as "A..B", both A
  and B are included in the range) or "DEF" (a synonym of "0..255").

An example of a dot file that can be parsed by this script:
digraph one_instruction_x86_64 {
	node [ shape = point ];
	ENTRY;
	node [ fixedsize = true, height = 0.65, shape = doublecircle ];
	3;
	node [ shape = circle ];
	1 -> 2 [ label = "0..3 / action_1" ];
	2 -> 3 [ label = "DEF / action_2" ];
	1 -> 3 [ label = "15" ];
	ENTRY -> 1 [ label = "IN" ];
}
"""

import re
import sys


# Actions that trigger begin/end of a multibyte field that does not affect
# the structure of a command.
anyfield_begin_actions = [
  'disp8_operand_begin', 'disp32_operand_begin', 'disp64_operand_begin',
  'rel8_operand_begin', 'rel16_operand_begin', 'rel32_operand_begin',
  'imm8_operand_begin', 'imm16_operand_begin', 'imm32_operand_begin',
  'imm64_operand_begin'
]

anyfield_end_actions = [
  'disp8_operand_end', 'disp32_operand_end', 'disp64_operand_end',
  'rel8_operand_end', 'rel16_operand_end', 'rel32_operand_end',
  'imm8_operand_end', 'imm16_operand_end', 'imm32_operand_end',
  'imm64_operand_end'
]


def CheckOnlyDefaultState(states, st):
  """Asserts that there is only one transition, the default one."""

  assert(states[st]['default_to'] != None and
      len(states[st]['transitions']) == 0)


def InitStateIfNeeded(states, st):
  """Initialize the information necessary for each state if it was not done.

  Each state is represented by the dictionary with keys:
    'is_final': whether the state is final
    'anyfield_begin': whether this state starts a multibyte field in an
        instruction
    'anyfield_end': whether this state ends a multibyte field
    'default_to': a name of a state to transition to by default (i.e. if no
        value from explicit transitions matches)
    'transitions': A list of transitions. Each transition is a dictionary with
    keys:
      'begin_byte': the byte that begins the range of bytes, inclusive
      'end_byte': the byte that ends the range, inclusive
      'state_to': name of the state to transition to

  Args:
      states: The dict containing information associated with each state.
      st: The name of the state.

  Returns:
      A state. For example:
      {'is_final': False,
       'anyfield_begin': False,
       'anyfield_end': False,
       'transitions': [{'begin_byte': '0',
                        'end_byte': '0',
                        'state_to': '100',
                        }],
       'default_to': '200',
       }
  """

  if not states.get(st, None):
    states[st] = {'is_final': False,
                  'transitions': [],
                  'anyfield_begin': False,
                  'anyfield_end': False,
                  'default_to': None,
                  }
  return states[st]


def Main():
  met_finals = False
  started_edges = False
  states = {}
  max_state = 0
  for line in sys.stdin.readlines():
    # Find out which states are final.
    if not started_edges:
      if met_finals:
        if re.match(r'\tnode \[', line):
          started_edges = True
          continue
        final_state_m = re.match(r'\t(\w+);', line)
        assert(final_state_m)
        st_name = final_state_m.group(1)
        InitStateIfNeeded(states, st_name)['is_final'] = True
      if not met_finals and re.match(r'\tnode .* shape = doublecircle', line):
        met_finals = True
      continue

    # Parse an edge or state.
    edge_m = re.match(r'\t(\w+) -> (\w+) \[([^\]]+)\];$', line)
    if edge_m:
      state_from = edge_m.group(1)
      state_to = edge_m.group(2)
      max_state = max(max_state, int(state_to))
      try:
        max_state = max(max_state, int(state_from))
      except ValueError:
        if state_from == 'ENTRY':
          start_state = state_to
          continue

      # Parse edge label.
      text_m = re.match(r' label = "([^"]+)" ', edge_m.group(3))
      assert(text_m)
      label_text = text_m.group(1)

      while True:
        target_m = re.match(r'(, )?(\w+\.\.\w+|\w+)(.*)$',
                            label_text)
        if not target_m:
          break
        label_text = target_m.group(3)
        byte_range_text = target_m.group(2)
        byte_range_m = re.match(r'(\w+)\.\.(\w+)', byte_range_text)
        if byte_range_text == 'DEF':
          InitStateIfNeeded(states, state_from)['default_to'] = state_to
        else:
          if byte_range_m:
            begin_byte = byte_range_m.group(1)
            end_byte = byte_range_m.group(2)
          else:
            begin_byte = byte_range_text
            end_byte = begin_byte
          InitStateIfNeeded(states, state_from)
          InitStateIfNeeded(states, state_to)
          states[state_from]['transitions'].append({'begin_byte': begin_byte,
                                                    'end_byte': end_byte,
                                                    'state_to': state_to,
                                                    })
      while True:
        action_m = re.match('( / |, )(\w+)(.*)$', label_text)
        if not action_m:
          break
        label_text = action_m.group(3)
        action_name = action_m.group(2)
        if action_name in anyfield_begin_actions:
          InitStateIfNeeded(states, state_from)['anyfield_begin'] = True
          CheckOnlyDefaultState(states, state_from)
        if action_name in anyfield_end_actions:
          InitStateIfNeeded(states, state_from)['anyfield_end'] = True
          CheckOnlyDefaultState(states, state_from)

  # Generate the C definitions based on parsed dot file.
  print '/* This code was generated by parse_dfa.py.  */'
  print
  print '#include "test_dfa.h"'
  print
  print 'struct state states[] = {'
  def BoolToStr(bool_value):
    ret = 'false'
    if bool_value:
      ret = ' true'
    return ret
  for i in xrange(max_state + 1):
    if not states.get(str(i),None):
      print ('  { .is_final = false, .anyfield_begin = false,' +
             ' .anyfield_end = false }, /* %d */' % i)
      continue
    st = states[str(i)]
    print ('  { .is_final = %s, .anyfield_begin = %s,' +
           ' .anyfield_end = %s }, /* %d */') % (
        BoolToStr(st['is_final']),
        BoolToStr(st['anyfield_begin']),
        BoolToStr(st['anyfield_end']),
        i)
  print '};'
  print

  print 'void InitTransitions() {'
  for i in xrange(max_state + 1):
    if states.get(str(i),None):
      default_to = states[str(i)]['default_to']
      default_transition_only = True
      default_bytes = [True] * 256
      for tr in states[str(i)]['transitions']:
        default_transition_only = False
        for j in xrange(int(tr['begin_byte']), int(tr['end_byte']) + 1):
          default_bytes[j] = False
        print 'AddRange({0}, {1}, {2}, {3});'.format(
            str(i), tr['begin_byte'], tr['end_byte'], tr['state_to'])
      if default_to != None and default_transition_only:
        print 'AddRange({0}, {1}, {2}, {3});'.format(
            str(i), '0', '255', default_to)
      elif default_to != None:
        for j in xrange(0, 256):
          if default_bytes[j]:
            print 'AddRange({0}, {1}, {2}, {3});'.format(
                str(i), str(j), str(j), default_to)

  print '}'
  print
  print 'uint16_t start_state = {0};'.format(start_state)
  print
  print 'uint16_t NumStates() { return %d; }' % (max_state + 1)


if __name__ == '__main__':
  sys.exit(Main())
