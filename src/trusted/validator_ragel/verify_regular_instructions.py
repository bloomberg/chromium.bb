#!/usr/bin/python
# Copyright (c) 2013 The Native Client Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""
Generate all acceptable regular instructions by traversing validator DFA
and run objdump, new and old validator on them.
"""

import multiprocessing
import optparse

import dfa_parser
import dfa_traversal
import validator


class WorkerState(object):
  def __init__(self, prefix):
    self.total_instructions = 0

  def ReceiveInstruction(self, s):
    pass

  def Finish(self):
    pass


def Worker((prefix, state_index)):
  worker_state = WorkerState(prefix)

  try:
    dfa_traversal.TraverseTree(
        states[state_index],
        final_callback=worker_state.ReceiveInstruction,
        prefix=prefix)
  finally:
    worker_state.Finish()

  return prefix, worker_state.total_instructions


def ParseOptions():
  parser = optparse.OptionParser(usage='%prog [options] xmlfile')

  parser.add_option('-b', '--bitness',
                    type=int,
                    help='The subarchitecture: 32 or 64')
  parser.add_option('-a', '--gas',
                    help='Path to GNU AS executable')
  parser.add_option('-d', '--objdump',
                    help='Path to objdump executable')
  parser.add_option('-v', '--validator_dll',
                    help='Path to librdfa_validator_dll')

  options, args = parser.parse_args()

  if options.bitness not in [32, 64]:
    parser.error('specify -b 32 or -b 64')

  if not (options.gas and options.objdump and options.validator_dll):
    parser.error('specify path to gas, objdump, and validator_dll')

  if len(args) != 1:
    parser.error('specify one xml file')

  (xml_file,) = args

  return options, xml_file


options, xml_file = ParseOptions()
# We are doing it here to share state graph between workers spawned by
# multiprocess. Passing it every time is slow.
states, initial_state = dfa_parser.ParseXml(xml_file)

validator.Init(options.validator_dll)


def main():
  assert initial_state.is_accepting
  assert not initial_state.any_byte

  print len(states), 'states'

  num_suffixes = dfa_traversal.GetNumSuffixes(initial_state)

  # We can't just write 'num_suffixes[initial_state]' because
  # initial state is accepting.
  total_instructions = sum(num_suffixes[t.to_state]
                           for t in initial_state.forward_transitions.values())
  print total_instructions, 'regular instructions total'

  tasks = dfa_traversal.CreateTraversalTasks(states, initial_state)
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
