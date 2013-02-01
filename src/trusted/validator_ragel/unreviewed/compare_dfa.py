#!/usr/bin/python
# Copyright (c) 2013 The Native Client Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import sys
import os
sys.path.append(os.path.join(os.path.dirname(__file__), '..'))
import dfa_parser


visited_pairs = set()


def Traverse(state1, state2, path):
  if (state1, state2) in visited_pairs:
    return

  if state1.is_accepting != state2.is_accepting:
    print map(hex, path)
    print state1.is_accepting
    print state2.is_accepting
    sys.exit(1)

  visited_pairs.add((state1, state2))

  for byte in range(256):
    new_path = path + [byte]

    t1 = state1.forward_transitions.get(byte)
    t2 = state2.forward_transitions.get(byte)
    if (t1 is None) != (t2 is None):
      print map(hex, new_path)
      print t1 is not None
      print t2 is not None
      sys.exit(1)

    if t1 is None:
      continue

    Traverse(t1.to_state, t2.to_state, new_path)


def main():
  filename1, filename2 = sys.argv[1:]

  _, start_state1 = dfa_parser.ParseXml(filename1)
  _, start_state2 = dfa_parser.ParseXml(filename2)

  Traverse(start_state1, start_state2, [])

  print 'automata are equivalent'


if __name__ == '__main__':
  main()
