#!/usr/bin/env python
# Copyright (c) 2011 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Unit tests for subprocess2.py."""

import optparse
import os
import sys
import time
import unittest

ROOT_DIR = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, ROOT_DIR)

import subprocess2


class Subprocess2Test(unittest.TestCase):
  # Can be mocked in a test.
  TO_SAVE = ['Popen', 'call', 'check_call', 'capture', 'check_output']

  def setUp(self):
    self.exe_path = __file__
    self.exe = [self.exe_path, '--child']
    self.saved = dict(
        (name, getattr(subprocess2, name)) for name in self.TO_SAVE)

  def tearDown(self):
    for name, value in self.saved.iteritems():
      setattr(subprocess2, name, value)

  @staticmethod
  def _prep():
    results = {}
    def fake_call(args, **kwargs):
      results.update(kwargs)
      results['args'] = args
      return ['stdout', 'stderr'], 0
    subprocess2.call = fake_call
    return results

  def test_check_call_defaults(self):
    results = self._prep()
    self.assertEquals(
        ['stdout', 'stderr'], subprocess2.check_call(['foo'], a=True))
    expected = {
        'args': ['foo'],
        'a':True,
    }
    self.assertEquals(expected, results)

  def test_check_output_defaults(self):
    results = self._prep()
    # It's discarding 'stderr' because it assumes stderr=subprocess2.STDOUT but
    # fake_call() doesn't 'implement' that.
    self.assertEquals('stdout', subprocess2.check_output(['foo'], a=True))
    expected = {
        'args': ['foo'],
        'a':True,
        'stdout': subprocess2.PIPE,
        'stderr': subprocess2.STDOUT,
    }
    self.assertEquals(expected, results)

  def test_timeout(self):
    # It'd be better to not discard stdout.
    out, returncode = subprocess2.call(
        self.exe + ['--sleep', '--stdout'],
        timeout=0.01,
        stdout=subprocess2.PIPE)
    self.assertEquals(-9, returncode)
    self.assertEquals(['', None], out)

  def test_void(self):
    out = subprocess2.check_output(
         self.exe + ['--stdout', '--stderr'],
         stdout=subprocess2.VOID)
    self.assertEquals(None, out)
    out = subprocess2.check_output(
         self.exe + ['--stdout', '--stderr'],
         stderr=subprocess2.VOID)
    self.assertEquals('A\nBB\nCCC\n', out)

  def test_check_output_throw(self):
    try:
      subprocess2.check_output(self.exe + ['--fail', '--stderr'])
      self.fail()
    except subprocess2.CalledProcessError, e:
      self.assertEquals('a\nbb\nccc\n', e.stdout)
      self.assertEquals(None, e.stderr)
      self.assertEquals(64, e.returncode)

  def test_check_call_throw(self):
    try:
      subprocess2.check_call(self.exe + ['--fail', '--stderr'])
      self.fail()
    except subprocess2.CalledProcessError, e:
      self.assertEquals(None, e.stdout)
      self.assertEquals(None, e.stderr)
      self.assertEquals(64, e.returncode)


def child_main(args):
  parser = optparse.OptionParser()
  parser.add_option(
      '--fail',
      dest='return_value',
      action='store_const',
      default=0,
      const=64)
  parser.add_option('--stdout', action='store_true')
  parser.add_option('--stderr', action='store_true')
  parser.add_option('--sleep', action='store_true')
  options, args = parser.parse_args(args)
  if args:
    parser.error('Internal error')

  def do(string):
    if options.stdout:
      print >> sys.stdout, string.upper()
    if options.stderr:
      print >> sys.stderr, string.lower()

  do('A')
  do('BB')
  do('CCC')
  if options.sleep:
    time.sleep(10)
  return options.return_value


if __name__ == '__main__':
  if len(sys.argv) > 1 and sys.argv[1] == '--child':
    sys.exit(child_main(sys.argv[2:]))
  unittest.main()
