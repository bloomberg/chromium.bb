#!/usr/bin/env python
# Copyright 2013 The Swarming Authors. All rights reserved.
# Use of this source code is governed under the Apache License, Version 2.0 that
# can be found in the LICENSE file.

import itertools
import logging
import os
import sys
import unittest

ROOT_DIR = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, ROOT_DIR)

from utils import subprocess42

OUTPUT = os.path.join(ROOT_DIR, 'tests', 'subprocess42', 'output.py')


# Disable pre-set unbuffered output to not interfere with the testing being done
# here. Otherwise everything would test with unbuffered; which is fine but
# that's not what we specifically want to test here.
ENV = os.environ.copy()
ENV.pop('PYTHONUNBUFFERED', None)


SCRIPT = (
  'import signal, sys, time;\n'
  'l = [];\n'
  'def handler(signum, _):\n'
  '  l.append(signum);\n'
  '  print(\'got signal %%d\' %% signum);\n'
  '  sys.stdout.flush();\n'
  'signal.signal(%s, handler);\n'
  'print(\'hi\');\n'
  'sys.stdout.flush();\n'
  'while not l:\n'
  '  try:\n'
  '    time.sleep(0.01);\n'
  '  except IOError:\n'
  '    print(\'ioerror\');\n'
  'print(\'bye\')') % (
    'signal.SIGBREAK' if sys.platform == 'win32' else 'signal.SIGTERM')


def to_native_eol(string):
  if string is None:
    return string
  if sys.platform == 'win32':
    return string.replace('\n', '\r\n')
  return string


def get_output_sleep_proc(flush, unbuffered, sleep_duration):
  """Returns process with universal_newlines=True that prints to stdout before
  after a sleep.

  It also optionally sys.stdout.flush() before the sleep and optionally enable
  unbuffered output in python.
  """
  command = [
    'import sys,time',
    'print(\'A\')',
  ]
  if flush:
    # Sadly, this doesn't work otherwise in some combination.
    command.append('sys.stdout.flush()')
  command.extend((
    'time.sleep(%s)' % sleep_duration,
    'print(\'B\')',
  ))
  cmd = [sys.executable, '-c', ';'.join(command)]
  if unbuffered:
    cmd.append('-u')
  return subprocess42.Popen(
      cmd, env=ENV, stdout=subprocess42.PIPE, universal_newlines=True)


def get_output_sleep_proc_err(sleep_duration):
  """Returns process with universal_newlines=True that prints to stderr before
  and after a sleep.
  """
  command = [
    'import sys,time',
    'sys.stderr.write(\'A\\n\')',
  ]
  command.extend((
    'time.sleep(%s)' % sleep_duration,
    'sys.stderr.write(\'B\\n\')',
  ))
  cmd = [sys.executable, '-c', ';'.join(command)]
  return subprocess42.Popen(
      cmd, env=ENV, stderr=subprocess42.PIPE, universal_newlines=True)


class Subprocess42Test(unittest.TestCase):
  def test_recv_any(self):
    # Test all pipe direction and output scenarios.
    combinations = [
      {
        'cmd': ['out_print', 'err_print'],
        'stdout': None,
        'stderr': None,
        'expected': {},
      },
      {
        'cmd': ['out_print', 'err_print'],
        'stdout': None,
        'stderr': subprocess42.STDOUT,
        'expected': {},
      },

      {
        'cmd': ['out_print'],
        'stdout': subprocess42.PIPE,
        'stderr': subprocess42.PIPE,
        'expected': {'stdout': 'printing'},
      },
      {
        'cmd': ['out_print'],
        'stdout': subprocess42.PIPE,
        'stderr': None,
        'expected': {'stdout': 'printing'},
      },
      {
        'cmd': ['out_print'],
        'stdout': subprocess42.PIPE,
        'stderr': subprocess42.STDOUT,
        'expected': {'stdout': 'printing'},
      },

      {
        'cmd': ['err_print'],
        'stdout': subprocess42.PIPE,
        'stderr': subprocess42.PIPE,
        'expected': {'stderr': 'printing'},
      },
      {
        'cmd': ['err_print'],
        'stdout': None,
        'stderr': subprocess42.PIPE,
        'expected': {'stderr': 'printing'},
      },
      {
        'cmd': ['err_print'],
        'stdout': subprocess42.PIPE,
        'stderr': subprocess42.STDOUT,
        'expected': {'stdout': 'printing'},
      },

      {
        'cmd': ['out_print', 'err_print'],
        'stdout': subprocess42.PIPE,
        'stderr': subprocess42.PIPE,
        'expected': {'stderr': 'printing', 'stdout': 'printing'},
      },
      {
        'cmd': ['out_print', 'err_print'],
        'stdout': subprocess42.PIPE,
        'stderr': subprocess42.STDOUT,
        'expected': {'stdout': 'printingprinting'},
      },
    ]
    for i, testcase in enumerate(combinations):
      cmd = [sys.executable, OUTPUT] + testcase['cmd']
      p = subprocess42.Popen(
          cmd, env=ENV, stdout=testcase['stdout'], stderr=testcase['stderr'])
      actual = {}
      while p.poll() is None:
        pipe, data = p.recv_any()
        if data:
          actual.setdefault(pipe, '')
          actual[pipe] += data

      # The process exited, read any remaining data in the pipes.
      while True:
        pipe, data = p.recv_any()
        if pipe is None:
          break
        actual.setdefault(pipe, '')
        actual[pipe] += data
      self.assertEqual(
          testcase['expected'],
          actual,
          (i, testcase['cmd'], testcase['expected'], actual))
      self.assertEqual((None, None), p.recv_any())
      self.assertEqual(0, p.returncode)

  def test_recv_any_different_buffering(self):
    # Specifically test all buffering scenarios.
    for flush, unbuffered in itertools.product([True, False], [True, False]):
      actual = ''
      proc = get_output_sleep_proc(flush, unbuffered, 0.5)
      while True:
        p, data = proc.recv_any()
        if not p:
          break
        self.assertEqual('stdout', p)
        self.assertTrue(data, (p, data))
        actual += data

      self.assertEqual('A\nB\n', actual)
      # Contrary to yield_any() or recv_any(0), wait() needs to be used here.
      proc.wait()
      self.assertEqual(0, proc.returncode)

  def test_recv_any_timeout_0(self):
    # rec_any() is expected to timeout and return None with no data pending at
    # least once, due to the sleep of 'duration' and the use of timeout=0.
    for flush, unbuffered in itertools.product([True, False], [True, False]):
      for duration in (0.05, 0.1, 0.5, 2):
        try:
          actual = ''
          proc = get_output_sleep_proc(flush, unbuffered, duration)
          got_none = False
          while True:
            p, data = proc.recv_any(timeout=0)
            if not p:
              if proc.poll() is None:
                got_none = True
                continue
              break
            self.assertEqual('stdout', p)
            self.assertTrue(data, (p, data))
            actual += data

          self.assertEqual('A\nB\n', actual)
          self.assertEqual(0, proc.returncode)
          self.assertEqual(True, got_none)
          break
        except AssertionError:
          if duration != 2:
            print('Sleeping rocks. Trying slower.')
            continue
          raise

  def _test_recv_common(self, proc, is_err):
    actual = ''
    while True:
      if is_err:
        data = proc.recv_err()
      else:
        data = proc.recv_out()
      if not data:
        break
      self.assertTrue(data)
      actual += data

    self.assertEqual('A\nB\n', actual)
    proc.wait()
    self.assertEqual(0, proc.returncode)

  def test_yield_any_no_timeout(self):
    for duration in (0.05, 0.1, 0.5, 2):
      try:
        proc = get_output_sleep_proc(True, True, duration)
        expected = [
          'A\n',
          'B\n',
        ]
        for p, data in proc.yield_any():
          self.assertEqual('stdout', p)
          self.assertEqual(expected.pop(0), data)
        self.assertEqual(0, proc.returncode)
        self.assertEqual([], expected)
        break
      except AssertionError:
        if duration != 2:
          print('Sleeping rocks. Trying slower.')
          continue
        raise

  def test_yield_any_timeout_0(self):
    # rec_any() is expected to timeout and return None with no data pending at
    # least once, due to the sleep of 'duration' and the use of timeout=0.
    for duration in (0.05, 0.1, 0.5, 2):
      try:
        proc = get_output_sleep_proc(True, True, duration)
        expected = [
          'A\n',
          'B\n',
        ]
        got_none = False
        for p, data in proc.yield_any(timeout=0):
          if not p:
            got_none = True
            continue
          self.assertEqual('stdout', p)
          self.assertEqual(expected.pop(0), data)
        self.assertEqual(0, proc.returncode)
        self.assertEqual([], expected)
        self.assertEqual(True, got_none)
        break
      except AssertionError:
        if duration != 2:
          print('Sleeping rocks. Trying slower.')
          continue
        raise

  def test_yield_any_timeout_0_called(self):
    # rec_any() is expected to timeout and return None with no data pending at
    # least once, due to the sleep of 'duration' and the use of timeout=0.
    for duration in (0.05, 0.1, 0.5, 2):
      try:
        proc = get_output_sleep_proc(True, True, duration)
        expected = [
          'A\n',
          'B\n',
        ]
        got_none = False
        called = []
        def timeout():
          called.append(0)
          return 0
        for p, data in proc.yield_any(timeout=timeout):
          if not p:
            got_none = True
            continue
          self.assertEqual('stdout', p)
          self.assertEqual(expected.pop(0), data)
        self.assertEqual(0, proc.returncode)
        self.assertEqual([], expected)
        self.assertEqual(True, got_none)
        self.assertTrue(called)
        break
      except AssertionError:
        if duration != 2:
          print('Sleeping rocks. Trying slower.')
          continue
        raise

  def test_detached(self):
    is_win = (sys.platform == 'win32')
    cmd = [sys.executable, '-c', SCRIPT]
    # TODO(maruel): Make universal_newlines=True work and not hang.
    proc = subprocess42.Popen(cmd, detached=True, stdout=subprocess42.PIPE)
    try:
      if is_win:
        # There's something extremely weird happening on the Swarming bots in
        # the sense that they return 'hi\n' on Windows, while running the script
        # locally on Windows results in 'hi\r\n'. Author raises fist.
        self.assertIn(proc.recv_out(), ('hi\r\n', 'hi\n'))
      else:
        self.assertEqual('hi\n', proc.recv_out())

      proc.terminate()

      if is_win:
        # What happens on Windows is that the process is immediately killed
        # after handling SIGBREAK.
        self.assertEqual(0, proc.wait())
        # Windows...
        self.assertIn(
            proc.recv_any(),
            (
              ('stdout', 'got signal 21\r\nioerror\r\nbye\r\n'),
              ('stdout', 'got signal 21\nioerror\nbye\n'),
              ('stdout', 'got signal 21\r\nbye\r\n'),
              ('stdout', 'got signal 21\nbye\n'),
            ))
      else:
        self.assertEqual(0, proc.wait())
        self.assertEqual(('stdout', 'got signal 15\nbye\n'), proc.recv_any())
    finally:
      # In case the test fails.
      proc.kill()

  def test_attached(self):
    is_win = (sys.platform == 'win32')
    cmd = [sys.executable, '-c', SCRIPT]
    # TODO(maruel): Make universal_newlines=True work and not hang.
    proc = subprocess42.Popen(cmd, detached=False, stdout=subprocess42.PIPE)
    try:
      if is_win:
        # There's something extremely weird happening on the Swarming bots in
        # the sense that they return 'hi\n' on Windows, while running the script
        # locally on Windows results in 'hi\r\n'. Author raises fist.
        self.assertIn(proc.recv_out(), ('hi\r\n', 'hi\n'))
      else:
        self.assertEqual('hi\n', proc.recv_out())

      proc.terminate()

      if is_win:
        # If attached, it's hard killed.
        self.assertEqual(1, proc.wait())
        self.assertEqual((None, None), proc.recv_any())
      else:
        self.assertEqual(0, proc.wait())
        self.assertEqual(('stdout', 'got signal 15\nbye\n'), proc.recv_any())
    finally:
      # In case the test fails.
      proc.kill()


if __name__ == '__main__':
  if '-v' in sys.argv:
    unittest.TestCase.maxDiff = None
  logging.basicConfig(
      level=logging.DEBUG if '-v' in sys.argv else logging.ERROR)
  unittest.main()
