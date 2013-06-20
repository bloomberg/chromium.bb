#!/usr/bin/python
# Copyright (c) 2013 The Native Client Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""
Generate all acceptable regular instructions by traversing validator DFA
and run check them against RDFA decoder and text-based specification.
"""

import itertools
import multiprocessing
import optparse
import os
import re
import subprocess
import sys
import tempfile
import traceback

import dfa_parser
import dfa_traversal
import objdump_parser
import validator
import spec


FWAIT = 0x9b
NOP = 0x90


def IsRexPrefix(byte):
  return 0x40 <= byte < 0x50


def ValidateInstruction(instruction):
  assert len(instruction) <= validator.BUNDLE_SIZE
  bundle = instruction + [NOP] * (validator.BUNDLE_SIZE - len(instruction))

  dis = validator.DisassembleChunk(
      ''.join(map(chr, instruction)),
      bitness=options.bitness)

  # Objdump (and consequently our decoder) displays fwait with rex prefix in
  # a rather strange way:
  #   0: 41      fwait
  #   1: 9b      fwait
  # So we manually convert it to
  #   0: 41 9b   fwait
  # for the purpose of validation.
  # TODO(shcherbina): get rid of this special handling once
  # https://code.google.com/p/nativeclient/issues/detail?id=3496 is fixed.
  if (len(instruction) == 2 and
      IsRexPrefix(instruction[0]) and
      instruction[1] == FWAIT):
    assert len(dis) == 2
    assert dis[0].disasm == dis[1].disasm == 'fwait'
    dis[0].bytes.extend(dis[1].bytes)
    del dis[1]

  assert len(dis) == 1, (instruction, dis)
  (dis,) = dis
  assert dis.bytes == instruction, (instruction, dis)

  if options.bitness == 32:
    result = validator.ValidateChunk(
        ''.join(map(chr, bundle)),
        bitness=options.bitness)

    try:
      spec.ValidateDirectJumpOrRegularInstruction(dis, bitness=32)
      if not result:
        print 'warning: validator rejected', dis
    except spec.SandboxingError as e:
      if result:
        print 'validator accepted instruction disallowed by specification'
        raise
    except spec.DoNotMatchError:
      # TODO(shcherbina): When text-based specification is complete,
      # it should raise.
      pass

    return result

  else:
    result = validator.ValidateChunk(
        ''.join(map(chr, bundle)),
        bitness=options.bitness)

    try:
      spec.ValidateDirectJumpOrRegularInstruction(dis, bitness=64)
      # TODO(shcherbina): handle restricted registers.
      if not result:
        print 'warning: validator rejected', dis
    except spec.SandboxingError as e:
      if result:
        print 'validator accepted instruction disallowed by specification'
        raise
    except spec.DoNotMatchError:
      # TODO(shcherbina): When text-based specification is complete,
      # it should raise.
      pass

    return result


class WorkerState(object):
  def __init__(self, prefix):
    self.total_instructions = 0
    self.num_valid = 0

  def ReceiveInstruction(self, bytes):
    self.total_instructions += 1
    self.num_valid += ValidateInstruction(bytes)


def Worker((prefix, state_index)):
  worker_state = WorkerState(prefix)

  try:
    dfa_traversal.TraverseTree(
        dfa.states[state_index],
        final_callback=worker_state.ReceiveInstruction,
        prefix=prefix,
        anyfield=0)
  except Exception as e:
    traceback.print_exc() # because multiprocessing imap swallows traceback
    raise

  return (
      prefix,
      worker_state.total_instructions,
      worker_state.num_valid)


def ParseOptions():
  parser = optparse.OptionParser(usage='%prog [options] xmlfile')

  parser.add_option('--bitness',
                    type=int,
                    help='The subarchitecture: 32 or 64')
  parser.add_option('--validator_dll',
                    help='Path to librdfa_validator_dll')
  parser.add_option('--decoder_dll',
                    help='Path to librdfa_decoder_dll')

  options, args = parser.parse_args()

  if options.bitness not in [32, 64]:
    parser.error('specify -b 32 or -b 64')

  if len(args) != 1:
    parser.error('specify one xml file')

  (xml_file,) = args

  return options, xml_file


options, xml_file = ParseOptions()
# We are doing it here to share state graph between workers spawned by
# multiprocess. Passing it every time is slow.
dfa = dfa_parser.ParseXml(xml_file)

validator.Init(
    validator_dll=options.validator_dll,
    decoder_dll=options.decoder_dll)


def main():
  assert dfa.initial_state.is_accepting
  assert not dfa.initial_state.any_byte

  print len(dfa.states), 'states'

  num_suffixes = dfa_traversal.GetNumSuffixes(dfa.initial_state)

  # We can't just write 'num_suffixes[dfa.initial_state]' because
  # initial state is accepting.
  total_instructions = sum(
      num_suffixes[t.to_state]
      for t in dfa.initial_state.forward_transitions.values())
  print total_instructions, 'regular instructions total'

  tasks = dfa_traversal.CreateTraversalTasks(dfa.states, dfa.initial_state)
  print len(tasks), 'tasks'

  pool = multiprocessing.Pool()

  results = pool.imap(Worker, tasks)

  total = 0
  num_valid = 0
  for prefix, count, valid_count in results:
    print ', '.join(map(hex, prefix))
    total += count
    num_valid += valid_count

  print total, 'instructions were processed'
  print num_valid, 'valid instructions'


if __name__ == '__main__':
  main()
