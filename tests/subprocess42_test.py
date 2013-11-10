#!/usr/bin/env python
# Copyright 2013 The Swarming Authors. All rights reserved.
# Use of this source code is governed under the Apache License, Version 2.0 that
# can be found in the LICENSE file.

import logging
import os
import sys
import unittest

ROOT_DIR = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, ROOT_DIR)

from utils import subprocess42

OUTPUT = os.path.join(ROOT_DIR, 'tests', 'subprocess42', 'output.py')


def to_native_eol(string):
  if string is None:
    return string
  if sys.platform == 'win32':
    return string.replace('\n', '\r\n')
  return string


class Subprocess42Test(unittest.TestCase):
  def test_call_with_timeout(self):
    timedout = 1 if sys.platform == 'win32' else -9
    # Format is:
    # ( (cmd, stderr_pipe, timeout), (stdout, stderr, returncode) ), ...
    test_data = [
      # 0 means no timeout, like None.
      (
        (['out_sleeping', '0.001', 'out_slept', 'err_print'], None, 0),
        ('Sleeping.\nSlept.\n', None, 0),
      ),
      (
        (['err_print'], subprocess42.STDOUT, 0),
        ('printing', None, 0),
      ),
      (
        (['err_print'], subprocess42.PIPE, 0),
        ('', 'printing', 0),
      ),

      # On a loaded system, this can be tight.
      (
        (['out_sleeping', 'out_flush', '100', 'out_slept'], None, 0.5),
        ('Sleeping.\n', '', timedout),
      ),
      (
        (
          # Note that err_flush is necessary on Windows but not on the other
          # OSes. This means the likelihood of missing stderr output from a
          # killed child process on Windows is much higher than on other OSes.
          [
            'out_sleeping', 'out_flush', 'err_print', 'err_flush', '100',
            'out_slept',
          ],
          subprocess42.PIPE,
          0.5),
        ('Sleeping.\n', 'printing', timedout),
      ),

      (
        (['out_sleeping', '0.001', 'out_slept'], None, 100),
        ('Sleeping.\nSlept.\n', '', 0),
      ),
    ]
    for i, (data, expected) in enumerate(test_data):
      stdout, stderr, code, duration = subprocess42.call_with_timeout(
          [sys.executable, OUTPUT] + data[0],
          stderr=data[1],
          timeout=data[2])
      self.assertTrue(duration > 0.0001, (data, duration))
      self.assertEqual(
          (i, stdout, stderr, code),
          (i,
            to_native_eol(expected[0]),
            to_native_eol(expected[1]),
            expected[2]))

      # Try again with universal_newlines=True.
      stdout, stderr, code, duration = subprocess42.call_with_timeout(
          [sys.executable, OUTPUT] + data[0],
          stderr=data[1],
          timeout=data[2],
          universal_newlines=True)
      self.assertTrue(duration > 0.0001, (data, duration))
      self.assertEqual(
          (i, stdout, stderr, code),
          (i,) + expected)

  def test_recv_any(self):
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
    for i, data in enumerate(combinations):
      cmd = [sys.executable, OUTPUT] + data['cmd']
      p = subprocess42.Popen(
          cmd, stdout=data['stdout'], stderr=data['stderr'])
      actual = {}
      while p.poll() is None:
        pipe, d = p.recv_any()
        if pipe is not None:
          actual.setdefault(pipe, '')
          actual[pipe] += d
      while True:
        pipe, d = p.recv_any()
        if pipe is None:
          break
        actual.setdefault(pipe, '')
        actual[pipe] += d
      self.assertEqual(
          data['expected'], actual, (i, data['cmd'], data['expected'], actual))
      self.assertEqual((None, None), p.recv_any())
      self.assertEqual(0, p.returncode)

  @staticmethod
  def _get_output_sleep_proc(flush, env, duration):
    command = [
      'import sys,time',
      'print(\'A\')',
    ]
    if flush:
      # Sadly, this doesn't work otherwise in some combination.
      command.append('sys.stdout.flush()')
    command.extend((
      'time.sleep(%s)' % duration,
      'print(\'B\')',
    ))
    return subprocess42.Popen(
        [
          sys.executable,
          '-c',
          ';'.join(command),
        ],
        stdout=subprocess42.PIPE,
        universal_newlines=True,
        env=env)

  def test_yield_any_None(self):
    for duration in (0.05, 0.1, 0.5, 2):
      try:
        proc = self._get_output_sleep_proc(True, {}, duration)
        expected = [
          'A\n',
          'B\n',
        ]
        for p, data in proc.yield_any(timeout=None):
          self.assertEqual('stdout', p)
          self.assertEqual(expected.pop(0), data)
        self.assertEqual(0, proc.returncode)
        self.assertEqual([], expected)
        break
      except AssertionError:
        if duration != 2:
          print('Sleeping rocks. trying more slowly.')
          continue
        raise

  def test_yield_any_0(self):
    for duration in (0.05, 0.1, 0.5, 2):
      try:
        proc = self._get_output_sleep_proc(True, {}, duration)
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
          print('Sleeping rocks. trying more slowly.')
          continue
        raise

  def test_recv_any_None(self):
    values = (
        (True, ['A\n', 'B\n'], {}),
        (False, ['A\nB\n'], {}),
        (False, ['A\n', 'B\n'], {'PYTHONUNBUFFERED': 'x'}),
    )
    for flush, exp, env in values:
      for duration in (0.05, 0.1, 0.5, 2):
        expected = exp[:]
        try:
          proc = self._get_output_sleep_proc(flush, env, duration)
          while True:
            p, data = proc.recv_any(timeout=None)
            if not p:
              break
            self.assertEqual('stdout', p)
            if not expected:
              self.fail(data)
            e = expected.pop(0)
            if env:
              # Buffering is truly a character-level and could get items
              # individually. This is usually seen only during high load, try
              # compiling at the same time to reproduce it.
              if len(data) < len(e):
                expected.insert(0, e[len(data):])
                e = e[:len(data)]
            self.assertEqual(e, data)
          # Contrary to yield_any() or recv_any(0), wait() needs to be used
          # here.
          proc.wait()
          self.assertEqual([], expected)
          self.assertEqual(0, proc.returncode)
        except AssertionError:
          if duration != 2:
            print('Sleeping rocks. trying more slowly.')
            continue
          raise

  def test_recv_any_0(self):
    values = (
        (True, ['A\n', 'B\n'], {}),
        (False, ['A\nB\n'], {}),
        (False, ['A\n', 'B\n'], {'PYTHONUNBUFFERED': 'x'}),
    )
    for i, (flush, exp, env) in enumerate(values):
      for duration in (0.1, 0.5, 2):
        expected = exp[:]
        try:
          proc = self._get_output_sleep_proc(flush, env, duration)
          got_none = False
          while True:
            p, data = proc.recv_any(timeout=0)
            if not p:
              if proc.poll() is None:
                got_none = True
                continue
              break
            self.assertEqual('stdout', p)
            if not expected:
              self.fail(data)
            e = expected.pop(0)
            if sys.platform == 'win32':
              # Buffering is truly a character-level on Windows and could get
              # items individually.
              if len(data) < len(e):
                expected.insert(0, e[len(data):])
                e = e[:len(data)]
            self.assertEqual(e, data)

          self.assertEqual(0, proc.returncode)
          self.assertEqual([], expected)
          self.assertEqual(True, got_none)
        except Exception as e:
          if duration != 2:
            print('Sleeping rocks. trying more slowly.')
            continue
          print >> sys.stderr, 'Failure at index %d' % i
          raise


if __name__ == '__main__':
  logging.basicConfig(
      level=logging.DEBUG if '-v' in sys.argv else logging.ERROR)
  unittest.main()
