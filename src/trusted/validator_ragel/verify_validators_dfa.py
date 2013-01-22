#!/usr/bin/python
# -*- coding: utf-8 -*-
# Copyright (c) 2012 The Native Client Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""%prog [option...] xmlfile

Simple checker for the validator's DFA."""

from __future__ import print_function

import itertools
import optparse
import sys

from lxml import etree


def ConvertXMLToStatesList(dfa_tree):
  """Reads state transtions from ragel XML file and returns python structure.

  Converts lxml-parsed XML to list of states (connected by forward_transitions
  and back_transitions).

  Args:
      dfa_tree: lxml-parsed ragel file

  Returns:
      List of State objects.
  """

  class State(object):
    __slots__ = [
        # Array of forward transtions (always 256 elements in size) for the
        # state:
        #   Nth element is used if byte N is encountered by a DFA
        #   If byte is not accepted then transtion is None
        'forward_transitions',
        # Backward transtions:
        #   List of transitions which can bring us to a given state
        'back_transitions',
        # True if state is an accepting state
        'is_accepting'
    ]


  class Transition(object):
    __slots__ = [
        # This transtion goes from the
        'from_state',
        # to the
        'to_state',
        # if
        'byte',
        # is encountered and it executes
        'action_table'
        # if that happens.
    ]

  xml_states = dfa_tree.xpath('//state')

  (error_state,) = dfa_tree.xpath('//error_state')
  error_state_id = int(error_state.text)

  states = [State() for _ in xml_states]

  for expected_state_id, xml_state in enumerate(xml_states):
    # Ragel always dumps states in order, but better to check that it's still so
    state_id = int(xml_state.get('id'))
    if state_id != expected_state_id:
      raise AssertionError('state_id ({0}) != expected state_id ({1})'.format(
          state_id, expected_state_id))

    state = states[state_id]
    state.forward_transitions = [None] * 256
    state.back_transitions = []
    state.is_accepting = bool(xml_state.get('final'))

    (xml_transitions,) = xml_state.xpath('trans_list')
    # Mark available transitions.
    for t in xml_transitions.getchildren():
      range_begin, range_end, next_state, action_table_id = t.text.split(' ')
      if next_state == 'x':
        # This is error action. We are supposed to have just one of these.
        assert action_table_id == '0'
      else:
        for byte in range(int(range_begin), int(range_end) + 1):
          transition = Transition()
          transition.from_state = states[state_id]
          transition.byte = byte
          transition.to_state = states[int(next_state)]
          if action_table_id == 'x':
            transition.action_table = None
          else:
            transition.action_table = int(action_table_id)
          state.forward_transitions[byte] = transition

    # State actions are only ever used in validator to detect error conditions
    # in non-final states.  Check that it's always so.
    state_actions = xml_state.xpath('state_actions')
    if state.is_accepting or state_id == error_state_id:
      assert len(state_actions) == 0
    else:
      assert len(state_actions) == 1
      (state_actions,) = state_actions
      assert state_actions.text == 'x x 0'

  for state in states:
    for transition in state.forward_transitions:
      # Ignore error transitions
      if transition is not None:
        transition.to_state.back_transitions.append(transition)

  return states


def CheckDFAActions(start_state, states):
  """Check action lists for accepting non-start states and identify all
  "dangerous" paths in the automaton.

  All paths in the DFA graph between two accepting states should fall into two
  classes:
    1. Path identical to the path from the start_state (including action lists).
       Such a path corresponds to "normal" instruction and are checked by
       separate test.
    2. Path which is not identical to any path from start_state ("dangerous"
       path).
       Such path is only valid if the final state is start_state and in this
       case one or more regular instructions must come before (situation is
       known as "superinstruction").  We list all these superinstructions here.

  For each "dangerous" path CollectSandboxing function is called. As a result,
  it prints list of superinstructions found and, more importantly, generates an
  assert error if DFA does not satisfy the requirements listed above.

  Note that situation when one register is restricted and then used by the next
  instruction to access memory is *not* considered a superinstruction and is
  *not* handled by the DFA.  Instead these cases are handled in actions via
  restricted_register variable (it's set if instruction restricts any register
  and if next command uses it then it's not a valid jump target).

  Args:
      start_state: start state of the DFA
      states: arrays of State structures (as produced by ConvertXMLToStatesList)
  Returns:
      None
  """

  for state in states:
    if state.is_accepting and state is not start_state:
      CheckPathActions(start_state, state, start_state, [], [])


def CheckPathActions(start_state,
                     state, main_state,
                     transitions, main_transitions):
  """Identify all "dangerous" paths that start with given prefix and check
  actions along them.

  "Dangerous" paths correspond to instructions that are not safe by themselves
  and should always be prefixed with some sandboxing instructions.

  In this traversal we rely on the fact that all such sandboxing instructions
  are "regular" (that is, are safe by themselves).

  This functions prints all the superinstructions which include "dangerous"
  instructions which start from "transitions" list and continue to "state".

  Args:
      start_state: start state of the DFA
      state: state to consider
      main_state: parallel state in path which starts from start_state
      transitions: list of transitions (path prefix collected so far)
      main_transitions: list of transitions (parallel path prefix starting from
                        start_state collected so far)
  Returns:
      None
  """

  if state.is_accepting and transitions != []:
    # If we reach accepting state, this path is not 'dangerous'.
    assert main_state.is_accepting
    return
  if state == main_state:
    # Original path merges with parallel path, so it can't be dangerous
    # (whatever is accepted further along original path would be accepted
    # from starting state as well).
    return
  for transition in state.forward_transitions:
    if transition is not None:
      main_transition = main_state.forward_transitions[transition.byte]
      if (main_transition is None or
          main_transition.action_table != transition.action_table):
        transitions.append(transition)
        CheckDangerousInstructionPath(start_state, transition.to_state,
                                      transitions)
        transitions.pop()
      else:
        transitions.append(transition)
        main_transitions.append(main_transition)
        CheckPathActions(start_state,
                         transition.to_state, main_transition.to_state,
                         transitions, main_transitions)
        main_transitions.pop()
        transitions.pop()


def CheckDangerousInstructionPath(start_state, state, transitions):
  """Enumerate all "dangerous" paths starting from given prefix and check them.

  We assume that we already diverged from the parallel path, so there is no need
  to track it anymore.

  A "dangerous" path is a path between two accepting states consuming a
  sequences of bytes S (e.g. the encoding of "jmp *%rax") such that these
  sequences S is not accepted when starting from start_start.

  Note that "dangerous" path does not necessarily split from "safe" paths on the
  first byte (for example "dangerous" instruction "jmp *%r8" is "0x41 0xff 0xe0"
  and "safe" instruction "inc %r8d" is "0x41 0xff 0xc0" share first two bytes
  and thus the first two transitions will be identical in the DFA).

  In validator's DFA it must finish at start_state and it needs some sandboxing.
  That is: it requires some fixed set of bytes present first for the DFA to
  recognize it as valid.  There are endless number of such possible sandboxing
  byte sequences but we only print the minimal ones (see the CollectSandboxing
  for the exact definition of what "minimal" means).  If it finishes in some
  other state (not in the start_state) then this function will stop at assert
  and if DFA is correct then it prints all the possible sandboxing sequences
  on stdout.

  Args:
      start_state: start state of the DFA
      state: state to consider
      transitions: transitions collected so far
  Returns:
      None
  """

  if state.is_accepting:
    assert state == start_state
    CollectSandboxing(start_state, transitions)
  else:
    for transition in state.forward_transitions:
      if transition is not None:
        transitions.append(transition)
        CheckDangerousInstructionPath(start_state, transition.to_state,
                                      transitions)
        transitions.pop()


def CollectSandboxing(start_state, transitions):
  """Collect sandboxing for the given "dangerous" instruction.

  Definition: sandboxing sequence for a given set of transitions is a sequence
  of bytes which are accepted by the DFA starting from the start_state which
  finishes with a given sequence of transitions.

  Many sandboxing sequences are suffixes of other sandboxing sequences.  For
  example sequence of bytes accept correspond to the sequence of instructions
  "and $~0x1f,%eax; and $~0x1f,%ebx; add %r15,%rbx; jmp *%rbx" is one such
  sequence (for the transtions which accept "jmp *%rbx"), but it includes a
  shorter one: "and $~0x1f,%ebx; add %r15,%rbx; jmp *%rbx".

  If any sandboxing sequence is suffix of some other sandboxing sequence and it
  triggers different actions then this function should assert, otherwise it
  prints all the minimal sandboxing sequences (sandboxings which are not
  suffixes of any other sandboxing sequences).

  It uses the following approach: if given sandboxing can be recognized starting
  from start_state and it can be recognized from some other state then we print
  it once, it needs some additional instructions then we go deeper.

  Args:
      start_state: start state of the DFA
      transitions: transitions collected so far
                   note: we go back in graph here, not forward
  Returns:
      None
  """

  if transitions[0].from_state.is_accepting:
    # We've added one additional instruction.  Let's see if this sequence is
    # recognizable from the beginning.
    main_state = start_state
    for transition in transitions:
      main_transition = main_state.forward_transitions[transition.byte]
      # No suitable transitions.  We need to go deeper.
      if main_transition is None:
        break
      else:
        assert main_transition.action_table == transition.action_table
        main_state = main_transition.to_state
    else:
      # Found the transition, but is it canonical one?
      assert main_state == start_state
      assert transitions[-1].to_state == start_state
      if transitions[0].from_state == start_state:
        print('.byte', ', '.join([str(t.byte) for t in transitions]))
      else:
        # This sequence was found in "shadow DFA".  Identical one should exist
        # in the main DFA - and will be printed at some point.
        main_state = start_state
        for transition in transitions:
          main_transition = main_state.forward_transitions[transition.byte]
          assert main_transition.action_table == transition.action_table
          main_state = main_transition.to_state
      return
  for transition in transitions[0].from_state.back_transitions:
    transitions.insert(0, transition)
    CollectSandboxing(start_state, transitions)
    transitions.pop(0)


def main(argv):

  # Options processing
  parser = optparse.OptionParser(__doc__)

  parser.add_option('-e', '--entry',
                    help='DFA of interest starts with entryname')

  (options, args) = parser.parse_args()

  if len(args) != 1:
    parser.error('need exactly one ragel XML file')

  # Open file
  xmlfile = args[0]
  dfa_tree = etree.parse(xmlfile)

  # Find entry point
  if options.entry is None:
    entries = dfa_tree.xpath('//entry')
    if len(entries) != 1:
      raise AssertionError('XML contains many entries and nothing is selected')
  else:
    entries = dfa_tree.xpath('//entry[@name={0}]'.format(options.entry))
    if len(entries) != 1:
      raise AssertionError('Can not find entry {0} in the XML'.format(entry))
  entry = int(entries[0].text)

  start_state = dfa_tree.xpath('//start_state')
  if len(start_state) != 1:
    raise AssertionError('Can not find the initial state in the XML')
  start_state = int(start_state[0].text)
  error_state = dfa_tree.xpath('//error_state')
  assert len(error_state) == 1 and int(error_state[0].text) == 0

  states = ConvertXMLToStatesList(dfa_tree)

  CheckDFAActions(states[start_state], states)

if __name__ == '__main__':
  exitcode = main(sys.argv)
  sys.exit(exitcode)
