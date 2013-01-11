#!/usr/bin/python
# Copyright (c) 2012 The Native Client Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os
import sys
import unittest

import gen_dfa


class TestLegacyPrefixes(unittest.TestCase):
  def test_empty(self):
    self.assertEquals(
        gen_dfa.GenerateLegacyPrefixes([], []),
        [()])

  def test_one(self):
    self.assertEquals(
        gen_dfa.GenerateLegacyPrefixes(['req'], []),
        [('req',)])
    self.assertEquals(
        set(gen_dfa.GenerateLegacyPrefixes([], ['opt'])),
        set([
            (),
            ('opt',)]))

  def test_many(self):
    self.assertEquals(
        set(gen_dfa.GenerateLegacyPrefixes(['req'], ['opt1', 'opt2'])),
        set([
            ('req',),
            ('opt1', 'req'),
            ('opt2', 'req'),
            ('req', 'opt1'),
            ('req', 'opt2'),
            ('opt1', 'opt2', 'req'),
            ('opt1', 'req', 'opt2'),
            ('opt2', 'opt1', 'req'),
            ('opt2', 'req', 'opt1'),
            ('req', 'opt1', 'opt2'),
            ('req', 'opt2', 'opt1')]))


class TestParser(unittest.TestCase):
  def test_instruction_definitions(self):
    def_filenames = sys.argv[1:]
    assert len(def_filenames) > 0
    for filename in def_filenames:
      gen_dfa.ParseDefFile(filename)


if __name__ == '__main__':
  unittest.main(argv=[sys.argv[0], '--verbose'])
