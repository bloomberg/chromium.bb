#!/usr/bin/python
# Copyright (c) 2012 The Native Client Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Tests that a set of symbols are truly pruned from the translator.

Compares the "on-device" translator with the "fat" host build.
"""

import os
import re
import subprocess
import sys
import unittest

class TestTranslatorPruned(unittest.TestCase):

  def my_check_output(self, cmd):
    # Local version of check_output() for compatibility
    # with python 2.6 (used by build bots).
    p = subprocess.Popen(cmd, stdout=subprocess.PIPE)
    stdout, _ = p.communicate()
    if p.returncode != 0:
      raise subprocess.CalledProcessError(p.returncode, cmd, stdout)
    return stdout

  def setUp(self):
    self.nm_tool = sys.argv[1]
    self.host_binary = sys.argv[2]
    self.target_binary = sys.argv[3]
    nm_unpruned_cmd = [self.nm_tool,
                       '--size-sort', '--demangle',
                       self.host_binary]
    self.unpruned_symbols = self.my_check_output(nm_unpruned_cmd).splitlines()
    nm_pruned_cmd = [self.nm_tool,
                     '--size-sort', '--demangle',
                     self.target_binary]
    self.pruned_symbols = self.my_check_output(nm_pruned_cmd).splitlines()
    # Make sure we're not trying to NM a stripped binary.
    self.assertTrue(len(self.unpruned_symbols) > 200)
    self.assertTrue(len(self.pruned_symbols) > 200)

  def sizeOfMatchingSyms(self, sym_regex, sym_list):
    # Check if a given sym_list has symbols matching sym_regex, and
    # return the total size of all matching symbols.
    total = 0
    for sym_info in sym_list:
      (hex_size, t, sym_name) = sym_info.split(' ', 2)
      if re.search(sym_regex, sym_name):
        total += int(hex_size, 16)
    return total

  def test_prunedNotFullyStripped(self):
    pruned = self.sizeOfMatchingSyms('stream_init.*NaClSrpc',
                                     self.pruned_symbols)
    self.assertNotEqual(pruned, 0)

  def test_didPrune(self):
    total = 0
    for sym_regex in ['LLParser', 'LLLexer',
                      'MCAsmParser', '::AsmParser',
                      'ARMAsmParser', 'X86AsmParser',
                      'ELFAsmParser', 'COFFAsmParser', 'DarwinAsmParser',
                      'MCAsmLexer', '::AsmLexer',
                      # Gigantic Asm MatchTable (globbed for all targets),
                      'MatchTable']:
      unpruned = self.sizeOfMatchingSyms(sym_regex, self.unpruned_symbols)
      pruned = self.sizeOfMatchingSyms(sym_regex, self.pruned_symbols)
      self.assertNotEqual(unpruned, 0, 'Unpruned never had ' + sym_regex)
      self.assertEqual(pruned, 0, 'Pruned still has ' + sym_regex)
      # Bytes pruned is approximate since the host build is different
      # from the target build (different inlining / optimizations).
      print 'Pruned out approx %d bytes worth of %s symbols' % (unpruned,
                                                                sym_regex)
      total += unpruned
    print 'Total %d bytes' % total

if __name__ == '__main__':
  if len(sys.argv) != 4:
    print 'Usage: %s <nm_tool> <unpruned_host_binary> <pruned_target_binary>'
    sys.exit(1)
  suite = unittest.TestLoader().loadTestsFromTestCase(TestTranslatorPruned)
  result = unittest.TextTestRunner(verbosity=2).run(suite)
  if result.wasSuccessful():
    sys.exit(0)
  else:
    sys.exit(1)
