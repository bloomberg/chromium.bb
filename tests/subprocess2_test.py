#!/usr/bin/env python
# Copyright (c) 2011 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Unit tests for subprocess2.py."""

import logging
import optparse
import os
import sys
import time
import unittest

try:
  import fcntl
except ImportError:
  fcntl = None

ROOT_DIR = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, ROOT_DIR)

import subprocess2

# Method could be a function
# pylint: disable=R0201


def convert_to_crlf(string):
  """Unconditionally convert LF to CRLF."""
  return string.replace('\n', '\r\n')


def convert_to_cr(string):
  """Unconditionally convert LF to CR."""
  return string.replace('\n', '\r')


def convert_win(string):
  """Converts string to CRLF on Windows only."""
  if sys.platform == 'win32':
    return string.replace('\n', '\r\n')
  return string


class DefaultsTest(unittest.TestCase):
  # Can be mocked in a test.
  TO_SAVE = {
    subprocess2: [
      'Popen', 'communicate', 'call', 'check_call', 'capture', 'check_output'],
    subprocess2.subprocess: ['Popen'],
  }

  def setUp(self):
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

  def test_timeout_shell_throws(self):
    # Never called.
    _ = self._fake_Popen()
    try:
      subprocess2.communicate(
          sys.executable,
          timeout=0.01,
          stdout=subprocess2.PIPE,
          shell=True)
      self.fail()
    except TypeError:
      pass


class S2Test(unittest.TestCase):
  def setUp(self):
    super(S2Test, self).setUp()
    self.exe_path = __file__
    self.exe = [sys.executable, self.exe_path, '--child']
    self.states = {}
    if fcntl:
      for v in (sys.stdin, sys.stdout, sys.stderr):
        fileno = v.fileno()
        self.states[fileno] = fcntl.fcntl(fileno, fcntl.F_GETFL)

  def tearDown(self):
    for fileno, fl in self.states.iteritems():
      self.assertEquals(fl, fcntl.fcntl(fileno, fcntl.F_GETFL))
    super(S2Test, self).tearDown()

  def _run_test(self, function):
    """Runs tests in 6 combinations:
    - LF output with universal_newlines=False
    - CR output with universal_newlines=False
    - CRLF output with universal_newlines=False
    - LF output with universal_newlines=True
    - CR output with universal_newlines=True
    - CRLF output with universal_newlines=True

    First |function| argument is the convertion for the origianl expected LF
    string to the right EOL.
    Second |function| argument is the executable and initial flag to run, to
    control what EOL is used by the child process.
    Third |function| argument is universal_newlines value.
    """
    noop = lambda x: x
    function(noop, self.exe, False)
    function(convert_to_cr, self.exe + ['--cr'], False)
    function(convert_to_crlf, self.exe + ['--crlf'], False)
    function(noop, self.exe, True)
    function(noop, self.exe + ['--cr'], True)
    function(noop, self.exe + ['--crlf'], True)

  def test_timeout(self):
    out, returncode = subprocess2.communicate(
        self.exe + ['--sleep_first', '--stdout'],
        timeout=0.01,
        stdout=subprocess2.PIPE,
        shell=False)
    self.assertEquals(subprocess2.TIMED_OUT, returncode)
    self.assertEquals(('', None), out)

  def test_check_output_no_stdout(self):
    try:
      subprocess2.check_output(self.exe, stdout=subprocess2.PIPE)
      self.fail()
    except TypeError:
      pass

  def test_stdout_void(self):
    def fn(c, e, un):
      (out, err), code = subprocess2.communicate(
          e + ['--stdout', '--stderr'],
          stdout=subprocess2.VOID,
          stderr=subprocess2.PIPE,
          universal_newlines=un)
      self.assertEquals(None, out)
      self.assertEquals(c('a\nbb\nccc\n'), err)
      self.assertEquals(0, code)
    self._run_test(fn)

  def test_stderr_void(self):
    def fn(c, e, un):
      (out, err), code = subprocess2.communicate(
          e + ['--stdout', '--stderr'],
          stdout=subprocess2.PIPE,
          stderr=subprocess2.VOID,
          universal_newlines=un)
      self.assertEquals(c('A\nBB\nCCC\n'), out)
      self.assertEquals(None, err)
      self.assertEquals(0, code)
    self._run_test(fn)

  def test_check_output_throw_stdout(self):
    def fn(c, e, un):
      try:
        subprocess2.check_output(
            e + ['--fail', '--stdout'], universal_newlines=un)
        self.fail()
      except subprocess2.CalledProcessError, e:
        self.assertEquals(c('A\nBB\nCCC\n'), e.stdout)
        self.assertEquals(None, e.stderr)
        self.assertEquals(64, e.returncode)
    self._run_test(fn)

  def test_check_output_throw_no_stderr(self):
    def fn(c, e, un):
      try:
        subprocess2.check_output(
            e + ['--fail', '--stderr'], universal_newlines=un)
        self.fail()
      except subprocess2.CalledProcessError, e:
        self.assertEquals(c(''), e.stdout)
        self.assertEquals(None, e.stderr)
        self.assertEquals(64, e.returncode)
    self._run_test(fn)

  def test_check_output_throw_stderr(self):
    def fn(c, e, un):
      try:
        subprocess2.check_output(
            e + ['--fail', '--stderr'], stderr=subprocess2.PIPE,
            universal_newlines=un)
        self.fail()
      except subprocess2.CalledProcessError, e:
        self.assertEquals('', e.stdout)
        self.assertEquals(c('a\nbb\nccc\n'), e.stderr)
        self.assertEquals(64, e.returncode)
    self._run_test(fn)

  def test_check_output_throw_stderr_stdout(self):
    def fn(c, e, un):
      try:
        subprocess2.check_output(
            e + ['--fail', '--stderr'], stderr=subprocess2.STDOUT,
            universal_newlines=un)
        self.fail()
      except subprocess2.CalledProcessError, e:
        self.assertEquals(c('a\nbb\nccc\n'), e.stdout)
        self.assertEquals(None, e.stderr)
        self.assertEquals(64, e.returncode)
    self._run_test(fn)

  def test_check_call_throw(self):
    try:
      subprocess2.check_call(self.exe + ['--fail', '--stderr'])
      self.fail()
    except subprocess2.CalledProcessError, e:
      self.assertEquals(None, e.stdout)
      self.assertEquals(None, e.stderr)
      self.assertEquals(64, e.returncode)

  def test_check_output_tee_stderr(self):
    def fn(c, e, un):
      stderr = []
      out, returncode = subprocess2.communicate(
          e + ['--stderr'], stderr=stderr.append,
          universal_newlines=un)
      self.assertEquals(c('a\nbb\nccc\n'), ''.join(stderr))
      self.assertEquals((None, None), out)
      self.assertEquals(0, returncode)
    self._run_test(fn)

  def test_check_output_tee_stdout_stderr(self):
    def fn(c, e, un):
      stdout = []
      stderr = []
      out, returncode = subprocess2.communicate(
          e + ['--stdout', '--stderr'],
          stdout=stdout.append,
          stderr=stderr.append,
          universal_newlines=un)
      self.assertEquals(c('A\nBB\nCCC\n'), ''.join(stdout))
      self.assertEquals(c('a\nbb\nccc\n'), ''.join(stderr))
      self.assertEquals((None, None), out)
      self.assertEquals(0, returncode)
    self._run_test(fn)

  def test_check_output_tee_stdin(self):
    def fn(c, e, un):
      stdout = []
      stdin = '0123456789'
      out, returncode = subprocess2.communicate(
          e + ['--stdout', '--read'], stdin=stdin, stdout=stdout.append,
          universal_newlines=un)
      self.assertEquals(c('A\nBB\nCCC\n'), ''.join(stdout))
      self.assertEquals((None, None), out)
      self.assertEquals(0, returncode)
    self._run_test(fn)

  def test_check_output_tee_throw(self):
    def fn(c, e, un):
      stderr = []
      try:
        subprocess2.check_output(
            e + ['--stderr', '--fail'], stderr=stderr.append,
            universal_newlines=un)
        self.fail()
      except subprocess2.CalledProcessError, e:
        self.assertEquals(c('a\nbb\nccc\n'), ''.join(stderr))
        self.assertEquals('', e.stdout)
        self.assertEquals(None, e.stderr)
        self.assertEquals(64, e.returncode)
    self._run_test(fn)

  def test_check_output_tee_large(self):
    stdout = []
    # Read 128kb. On my workstation it takes >2s. Welcome to 2011.
    out, returncode = subprocess2.communicate(
        self.exe + ['--large'], stdout=stdout.append)
    self.assertEquals(128*1024, len(''.join(stdout)))
    self.assertEquals((None, None), out)
    self.assertEquals(0, returncode)

  def test_check_output_tee_large_stdin(self):
    stdout = []
    # Write 128kb.
    stdin = '0123456789abcdef' * (8*1024)
    out, returncode = subprocess2.communicate(
        self.exe + ['--large', '--read'], stdin=stdin, stdout=stdout.append)
    self.assertEquals(128*1024, len(''.join(stdout)))
    self.assertEquals((None, None), out)
    self.assertEquals(0, returncode)


def child_main(args):
  if sys.platform == 'win32':
    # Annoying, make sure the output is not translated on Windows.
    # pylint: disable=E1101,F0401
    import msvcrt
    msvcrt.setmode(sys.stdout.fileno(), os.O_BINARY)
    msvcrt.setmode(sys.stderr.fileno(), os.O_BINARY)

  parser = optparse.OptionParser()
  parser.add_option(
      '--fail',
      dest='return_value',
      action='store_const',
      default=0,
      const=64)
  parser.add_option(
      '--crlf', action='store_const', const='\r\n', dest='eol', default='\n')
  parser.add_option(
      '--cr', action='store_const', const='\r', dest='eol')
  parser.add_option('--stdout', action='store_true')
  parser.add_option('--stderr', action='store_true')
  parser.add_option('--sleep_first', action='store_true')
  parser.add_option('--sleep_last', action='store_true')
  parser.add_option('--large', action='store_true')
  parser.add_option('--read', action='store_true')
  options, args = parser.parse_args(args)
  if args:
    parser.error('Internal error')
  if options.sleep_first:
    time.sleep(10)

  def do(string):
    if options.stdout:
      sys.stdout.write(string.upper())
      sys.stdout.write(options.eol)
    if options.stderr:
      sys.stderr.write(string.lower())
      sys.stderr.write(options.eol)

  do('A')
  do('BB')
  do('CCC')
  if options.large:
    # Print 128kb.
    string = '0123456789abcdef' * (8*1024)
    sys.stdout.write(string)
  if options.read:
    try:
      while sys.stdin.read():
        pass
    except OSError:
      pass
  if options.sleep_last:
    time.sleep(10)
  return options.return_value


if __name__ == '__main__':
  logging.basicConfig(level=
      [logging.WARNING, logging.INFO, logging.DEBUG][
        min(2, sys.argv.count('-v'))])
  if len(sys.argv) > 1 and sys.argv[1] == '--child':
    sys.exit(child_main(sys.argv[2:]))
  unittest.main()
