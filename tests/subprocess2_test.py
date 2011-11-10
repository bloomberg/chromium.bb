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

# Method could be a function
# pylint: disable=R0201

class Subprocess2Test(unittest.TestCase):
  # Can be mocked in a test.
  TO_SAVE = {
    subprocess2: [
      'Popen', 'communicate', 'call', 'check_call', 'capture', 'check_output'],
    subprocess2.subprocess: ['Popen'],
  }

  def setUp(self):
    self.exe_path = __file__
    self.exe = [sys.executable, self.exe_path, '--child']
    self.saved = {}
    for module, names in self.TO_SAVE.iteritems():
      self.saved[module] = dict(
          (name, getattr(module, name)) for name in names)
    # TODO(maruel): Do a reopen() on sys.__stdout__ and sys.__stderr__ so they
    # can be trapped in the child process for better coverage.

  def tearDown(self):
    for module, saved in self.saved.iteritems():
      for name, value in saved.iteritems():
        setattr(module, name, value)

  @staticmethod
  def _fake_communicate():
    results = {}
    def fake_communicate(args, **kwargs):
      assert not results
      results.update(kwargs)
      results['args'] = args
      return ['stdout', 'stderr'], 0
    subprocess2.communicate = fake_communicate
    return results

  @staticmethod
  def _fake_Popen():
    results = {}
    class fake_Popen(object):
      returncode = -8
      def __init__(self, args, **kwargs):
        assert not results
        results.update(kwargs)
        results['args'] = args
      def communicate(self):
        return None, None
    subprocess2.Popen = fake_Popen
    return results

  @staticmethod
  def _fake_subprocess_Popen():
    results = {}
    class fake_Popen(object):
      returncode = -8
      def __init__(self, args, **kwargs):
        assert not results
        results.update(kwargs)
        results['args'] = args
      def communicate(self):
        return None, None
    subprocess2.subprocess.Popen = fake_Popen
    return results

  def test_check_call_defaults(self):
    results = self._fake_communicate()
    self.assertEquals(
        ['stdout', 'stderr'], subprocess2.check_call_out(['foo'], a=True))
    expected = {
        'args': ['foo'],
        'a':True,
    }
    self.assertEquals(expected, results)

  def test_capture_defaults(self):
    results = self._fake_communicate()
    self.assertEquals(
        'stdout', subprocess2.capture(['foo'], a=True))
    expected = {
        'args': ['foo'],
        'a':True,
        'stdin': subprocess2.VOID,
        'stdout': subprocess2.PIPE,
    }
    self.assertEquals(expected, results)

  def test_communicate_defaults(self):
    results = self._fake_Popen()
    self.assertEquals(
        ((None, None), -8), subprocess2.communicate(['foo'], a=True))
    expected = {
        'args': ['foo'],
        'a': True,
    }
    self.assertEquals(expected, results)

  def test_Popen_defaults(self):
    results = self._fake_subprocess_Popen()
    proc = subprocess2.Popen(['foo'], a=True)
    self.assertEquals(-8, proc.returncode)
    expected = {
        'args': ['foo'],
        'a': True,
        'shell': bool(sys.platform=='win32'),
    }
    if sys.platform != 'win32':
      env = os.environ.copy()
      is_english = lambda name: env.get(name, 'en').startswith('en')
      if not is_english('LANG'):
        env['LANG'] = 'en_US.UTF-8'
        expected['env'] = env
      if not is_english('LANGUAGE'):
        env['LANGUAGE'] = 'en_US.UTF-8'
        expected['env'] = env
    self.assertEquals(expected, results)

  def test_check_output_defaults(self):
    results = self._fake_communicate()
    # It's discarding 'stderr' because it assumes stderr=subprocess2.STDOUT but
    # fake_communicate() doesn't 'implement' that.
    self.assertEquals('stdout', subprocess2.check_output(['foo'], a=True))
    expected = {
        'args': ['foo'],
        'a':True,
        'stdin': subprocess2.VOID,
        'stdout': subprocess2.PIPE,
    }
    self.assertEquals(expected, results)

  def test_timeout(self):
    # It'd be better to not discard stdout.
    out, returncode = subprocess2.communicate(
        self.exe + ['--sleep', '--stdout'],
        timeout=0.01,
        stdout=subprocess2.PIPE)
    self.assertEquals(subprocess2.TIMED_OUT, returncode)
    self.assertEquals(['', None], out)

  def test_check_output_no_stdout(self):
    try:
      subprocess2.check_output(self.exe, stdout=subprocess2.PIPE)
      self.fail()
    except TypeError:
      pass

  def test_stdout_void(self):
    (out, err), code = subprocess2.communicate(
         self.exe + ['--stdout', '--stderr'],
         stdout=subprocess2.VOID,
         stderr=subprocess2.PIPE)
    self.assertEquals(None, out)
    expected = 'a\nbb\nccc\n'
    if sys.platform == 'win32':
      expected = expected.replace('\n', '\r\n')
    self.assertEquals(expected, err)
    self.assertEquals(0, code)

  def test_stderr_void(self):
    (out, err), code = subprocess2.communicate(
         self.exe + ['--stdout', '--stderr'],
         universal_newlines=True,
         stdout=subprocess2.PIPE,
         stderr=subprocess2.VOID)
    self.assertEquals('A\nBB\nCCC\n', out)
    self.assertEquals(None, err)
    self.assertEquals(0, code)

  def test_check_output_throw_stdout(self):
    try:
      subprocess2.check_output(
          self.exe + ['--fail', '--stdout'], universal_newlines=True)
      self.fail()
    except subprocess2.CalledProcessError, e:
      self.assertEquals('A\nBB\nCCC\n', e.stdout)
      self.assertEquals(None, e.stderr)
      self.assertEquals(64, e.returncode)

  def test_check_output_throw_no_stderr(self):
    try:
      subprocess2.check_output(
          self.exe + ['--fail', '--stderr'], universal_newlines=True)
      self.fail()
    except subprocess2.CalledProcessError, e:
      self.assertEquals('', e.stdout)
      self.assertEquals(None, e.stderr)
      self.assertEquals(64, e.returncode)

  def test_check_output_throw_stderr(self):
    try:
      subprocess2.check_output(
          self.exe + ['--fail', '--stderr'], stderr=subprocess2.PIPE,
          universal_newlines=True)
      self.fail()
    except subprocess2.CalledProcessError, e:
      self.assertEquals('', e.stdout)
      self.assertEquals('a\nbb\nccc\n', e.stderr)
      self.assertEquals(64, e.returncode)

  def test_check_output_throw_stderr_stdout(self):
    try:
      subprocess2.check_output(
          self.exe + ['--fail', '--stderr'], stderr=subprocess2.STDOUT,
          universal_newlines=True)
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
