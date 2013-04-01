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
    print map(hex, path), state1.is_accepting
    print map(hex, path), state2.is_accepting
    sys.exit(1)

  visited_pairs.add((state1, state2))

  for byte in range(256):
    new_path = path + [byte]

    t1 = state1.forward_transitions.get(byte)
    t2 = state2.forward_transitions.get(byte)
    if (t1 is None) != (t2 is None):
      t = t1 or t2
      s = t.to_state
      path_to_accepting = new_path
      while not s.is_accepting:
        b = min(s.forward_transitions)
        path_to_accepting.append(b)
        s = s.forward_transitions[b].to_state

      if t1 is not None:
        print map(hex, path_to_accepting), True
        print map(hex, path), '...', False
      else:
        print map(hex, path), '...', False
        print map(hex, path_to_accepting), True
      sys.exit(1)

    if t1 is None:
      continue

    Traverse(t1.to_state, t2.to_state, new_path)


def main():
  filename1, filename2 = sys.argv[1:]

  dfa1 = dfa_parser.ParseXml(filename1)
  dfa2 = dfa_parser.ParseXml(filename2)

  Traverse(dfa1.initial_state, dfa2.initial_state, [])

  print 'automata are equivalent'


if __name__ == '__main__':
  main()
