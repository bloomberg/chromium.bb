#!/usr/bin/env python
# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import logging
import os
import shutil
import subprocess
import sys
import tempfile
import unittest

ROOT_DIR = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, ROOT_DIR)

import run_test_cases

OUTPUT = os.path.join(ROOT_DIR, 'tests', 'run_test_cases', 'output.py')


def to_native_eol(string):
  if string is None:
    return string
  if sys.platform == 'win32':
    return string.replace('\n', '\r\n')
  return string


class ListTestCasesTest(unittest.TestCase):
  def test_shards(self):
    test_cases = (
      (range(10), 10, 0, 1),

      ([0, 1], 5, 0, 3),
      ([2, 3], 5, 1, 3),
      ([4   ], 5, 2, 3),

      ([0], 5, 0, 7),
      ([1], 5, 1, 7),
      ([2], 5, 2, 7),
      ([3], 5, 3, 7),
      ([4], 5, 4, 7),
      ([ ], 5, 5, 7),
      ([ ], 5, 6, 7),

      ([0, 1], 4, 0, 2),
      ([2, 3], 4, 1, 2),
    )
    for expected, range_length, index, shards in test_cases:
      result = run_test_cases.filter_shards(range(range_length), index, shards)
      self.assertEquals(
          expected, result, (result, expected, range_length, index, shards))


def process_output(content, test_cases, duration, returncode):
  data = run_test_cases.process_output(content.splitlines(True), test_cases)
  return run_test_cases.normalize_testing_time(data, duration, returncode)


class RunTestCases(unittest.TestCase):
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
        (['err_print'], subprocess.STDOUT, 0),
        ('printing', None, 0),
      ),
      (
        (['err_print'], subprocess.PIPE, 0),
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
          subprocess.PIPE,
          0.5),
        ('Sleeping.\n', 'printing', timedout),
      ),

      (
        (['out_sleeping', '0.001', 'out_slept'], None, 100),
        ('Sleeping.\nSlept.\n', '', 0),
      ),
    ]
    for i, (data, expected) in enumerate(test_data):
      stdout, stderr, code, duration = run_test_cases.call_with_timeout(
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
      stdout, stderr, code, duration = run_test_cases.call_with_timeout(
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
        'stderr': subprocess.STDOUT,
        'expected': {},
      },

      {
        'cmd': ['out_print'],
        'stdout': subprocess.PIPE,
        'stderr': subprocess.PIPE,
        'expected': {'stdout': 'printing'},
      },
      {
        'cmd': ['out_print'],
        'stdout': subprocess.PIPE,
        'stderr': None,
        'expected': {'stdout': 'printing'},
      },
      {
        'cmd': ['out_print'],
        'stdout': subprocess.PIPE,
        'stderr': subprocess.STDOUT,
        'expected': {'stdout': 'printing'},
      },

      {
        'cmd': ['err_print'],
        'stdout': subprocess.PIPE,
        'stderr': subprocess.PIPE,
        'expected': {'stderr': 'printing'},
      },
      {
        'cmd': ['err_print'],
        'stdout': None,
        'stderr': subprocess.PIPE,
        'expected': {'stderr': 'printing'},
      },
      {
        'cmd': ['err_print'],
        'stdout': subprocess.PIPE,
        'stderr': subprocess.STDOUT,
        'expected': {'stdout': 'printing'},
      },

      {
        'cmd': ['out_print', 'err_print'],
        'stdout': subprocess.PIPE,
        'stderr': subprocess.PIPE,
        'expected': {'stderr': 'printing', 'stdout': 'printing'},
      },
      {
        'cmd': ['out_print', 'err_print'],
        'stdout': subprocess.PIPE,
        'stderr': subprocess.STDOUT,
        'expected': {'stdout': 'printingprinting'},
      },
    ]
    for data in combinations:
      cmd = [sys.executable, OUTPUT] + data['cmd']
      p = run_test_cases.Popen(
          cmd, stdout=data['stdout'], stderr=data['stderr'])
      actual = {}
      while p.poll() is None:
        got = p.recv_any()
        if got[0] is not None:
          actual.setdefault(got[0], '')
          actual[got[0]] += got[1]
      while True:
        got = p.recv_any()
        if got[0] is None:
          break
        actual.append(got)
      self.assertEqual(data['expected'], actual)
      self.assertEqual((None, None), p.recv_any())
      self.assertEquals(0, p.returncode)

  def test_gtest_filter(self):
    old = run_test_cases.run_test_cases
    exe = os.path.join(ROOT_DIR, 'tests', 'gtest_fake', 'gtest_fake_pass.py')
    def expect(
        executable, cwd, test_cases, jobs, timeout, clusters, retries,
        run_all, max_failures, no_cr, gtest_output, result_file, verbose):
      self.assertEqual(run_test_cases.fix_python_path([exe]), executable)
      self.assertEqual(os.getcwd(), cwd)
      # They are in reverse order due to test shuffling.
      self.assertEqual(['Foo.Bar1', 'Foo.Bar/3'], test_cases)
      self.assertEqual(run_test_cases.num_processors(), jobs)
      self.assertEqual(75, timeout)
      self.assertEqual(None, clusters)
      self.assertEqual(2, retries)
      self.assertEqual(None, run_all)
      self.assertEqual(None, no_cr)
      self.assertEqual('', gtest_output)
      self.assertEqual(None, max_failures)
      self.assertEqual(exe + '.run_test_cases', result_file)
      self.assertFalse(verbose)
      return 89

    try:
      run_test_cases.run_test_cases = expect
      result = run_test_cases.main([exe, '--gtest_filter=Foo.Bar*-*.Bar2'])
      self.assertEquals(89, result)
    finally:
      run_test_cases.run_test_cases = old

  def test_ResetableTimeout(self):
    self.assertTrue(run_test_cases.ResetableTimeout(0))
    a = run_test_cases.ResetableTimeout(1)
    self.assertEqual(1., float(a))
    count = 0
    for count in xrange(1000000):
      value = float(a)
      self.assertTrue(value >= 1., value)
      if value != 1.:
        break
      a.reset()
    self.assertTrue(value > 1., value)
    # Assume no 10s jank.
    self.assertTrue(value < 10., value)
    self.assertTrue(count < 1000000, count)
    self.assertTrue(count > 0, count)

  def test_convert_to_lines(self):
    data = [
      (
        ('blah'),
        ['blah'],
      ),
      (
        ('blah\n'),
        ['blah\n'],
      ),
      (
        ('blah', '\n'),
        ['blah\n'],
      ),
      (
        ('\n'),
        ['\n'],
      ),
      (
        ('blah blah\nboo'),
        ['blah blah\n', 'boo'],
      ),
      (
        ('b', 'lah blah\nboo'),
        ['blah blah\n', 'boo'],
      ),
    ]
    for generator, expected in data:
      self.assertEqual(
          expected,
          list(run_test_cases.convert_to_lines(generator)))

  def testRunSome(self):
    tests = [
        # Try with named arguments. Accepts 3*1 failures.
        (
          run_test_cases.RunSome(
              expected_count=10,
              retries=2,
              min_failures=1,
              max_failure_ratio=0.001,
              max_failures=None),
          [False] * 4),
        # Same without named arguments.
        (run_test_cases.RunSome(  10, 2, 1, 0.001, None), [False] * 4),

        (run_test_cases.RunSome(  10, 0, 1, 0.001, None), [False] * 2),
        (run_test_cases.RunSome(  10, 0, 1, 0.010, None), [False] * 2),

        # For low expected_count value, retries * min_failures is the minimum
        # bound of accepted failures.
        (run_test_cases.RunSome(  10, 2, 1, 0.010, None), [False] * 4),
        (run_test_cases.RunSome(  10, 2, 1, 0.020, None), [False] * 4),
        (run_test_cases.RunSome(  10, 2, 1, 0.050, None), [False] * 4),
        (run_test_cases.RunSome(  10, 2, 1, 0.100, None), [False] * 4),
        (run_test_cases.RunSome(  10, 2, 1, 0.110, None), [False] * 4),

        # Allows expected_count + retries failures at maximum.
        (run_test_cases.RunSome(  10, 2, 1, 0.200, None), [False] * 6),
        (run_test_cases.RunSome(  10, 2, 1, 0.999, None), [False] * 30),

        # The asympthote is nearing max_failure_ratio for large expected_count
        # values.
        (run_test_cases.RunSome(1000, 2, 1, 0.050, None), [False] * 150),
    ]
    for index, (decider, rounds) in enumerate(tests):
      for index2, r in enumerate(rounds):
        self.assertFalse(decider.should_stop(), (index, index2, str(decider)))
        decider.got_result(r)
      self.assertTrue(decider.should_stop(), (index, str(decider)))

  def testStatsInfinite(self):
    decider = run_test_cases.RunAll()
    for _ in xrange(200):
      self.assertFalse(decider.should_stop())
      decider.got_result(False)

  def test_process_output_garbage(self):
    data = 'garbage\n'
    expected = [
      {
        'duration': None,
        'output': None,
        'returncode': None,
        'test_case': 'Test.1',
      },
      {
        'duration': None,
        'output': None,
        'returncode': None,
        'test_case': 'Test.2',
      },
    ]
    actual = process_output(data, ['Test.1', 'Test.2'], 1.0, 0)
    self.assertEquals(expected, actual)

  def test_process_output_crash_cr(self):
    # CR only is supported. Let's assume a crash.
    data = '[ RUN      ] Test.1\r[ RUN      ] Test.2\r'
    expected = [
      {
        'crashed': True,
        'duration': 0,
        'output': '[ RUN      ] Test.1\r',
        'returncode': 1,
        'test_case': 'Test.1',
      },
      {
        'crashed': True,
        'duration': 1.,
        'output': '[ RUN      ] Test.2\r',
        'returncode': 1,
        'test_case': 'Test.2',
      },
    ]
    actual = process_output(data, ['Test.1', 'Test.2'], 1.0, 0)
    self.assertEquals(expected, actual)

  def test_process_output_crashes(self):
    data = '[ RUN      ] Test.1\n[ RUN      ] Test.2\n'
    expected = [
      {
        'crashed': True,
        'duration': 0,
        'output': '[ RUN      ] Test.1\n',
        'returncode': 1,
        'test_case': 'Test.1',
      },
      {
        'crashed': True,
        'duration': 1.,
        'output': '[ RUN      ] Test.2\n',
        'returncode': 1,
        'test_case': 'Test.2',
      },
    ]
    actual = process_output(data, ['Test.1', 'Test.2'], 1.0, 0)
    self.assertEquals(expected, actual)

  def test_process_output_ok(self):
    data = (
      '[ RUN      ] Test.1\n'
      '[       OK ] Test.1 (1000 ms)\n'
      '[ RUN      ] Test.2\n'
      '[       OK ] Test.2 (2000 ms)\n')
    expected = [
      {
        'duration': 11.,
        'output': '[ RUN      ] Test.1\n[       OK ] Test.1 (1000 ms)\n',
        'returncode': 0,
        'test_case': 'Test.1',
      },
      {
        'duration': 12.,
        'output': '[ RUN      ] Test.2\n[       OK ] Test.2 (2000 ms)\n',
        'returncode': 0,
        'test_case': 'Test.2',
      },
    ]
    actual = process_output(data, ['Test.1', 'Test.2'], 23.0, 0)
    self.assertEquals(expected, actual)

  def test_process_output_fail_1(self):
    data = (
      '[ RUN      ] Test.1\n'
      '[  FAILED  ] Test.1 (1000 ms)\n'
      '[ RUN      ] Test.2\n'
      '[       OK ] Test.2 (2000 ms)\n')
    expected = [
      {
        'duration': 11.,
        'output': '[ RUN      ] Test.1\n[  FAILED  ] Test.1 (1000 ms)\n',
        'returncode': 1,
        'test_case': 'Test.1',
      },
      {
        'duration': 12.,
        'output': '[ RUN      ] Test.2\n[       OK ] Test.2 (2000 ms)\n',
        'returncode': 0,
        'test_case': 'Test.2',
      },
    ]
    actual = process_output(data, ['Test.1', 'Test.2'], 23.0, 0)
    self.assertEquals(expected, actual)

  def test_process_output_crash_ok(self):
    data = (
      '[ RUN      ] Test.1\n'
      'blah blah crash.\n'
      '[ RUN      ] Test.2\n'
      '[       OK ] Test.2 (2000 ms)\n')
    expected = [
      {
        'crashed': True,
        'duration': 4.,
        'output': '[ RUN      ] Test.1\nblah blah crash.\n',
        'returncode': 1,
        'test_case': 'Test.1',
      },
      {
        'duration': 6.,
        'output': '[ RUN      ] Test.2\n[       OK ] Test.2 (2000 ms)\n',
        'returncode': 0,
        'test_case': 'Test.2',
      },
    ]
    actual = process_output(data, ['Test.1', 'Test.2'], 10.0, 0)
    self.assertEquals(expected, actual)

  def test_process_output_crash_garbage_ok(self):
    data = (
      '[ RUN      ] Test.1\n'
      'blah blah crash[ RUN      ] Test.2\n'
      '[       OK ] Test.2 (2000 ms)\n')
    expected = [
      {
        'crashed': True,
        'duration': 4.,
        'output': '[ RUN      ] Test.1\nblah blah crash',
        'returncode': 1,
        'test_case': 'Test.1',
      },
      {
        'duration': 6.,
        'output': '[ RUN      ] Test.2\n[       OK ] Test.2 (2000 ms)\n',
        'returncode': 0,
        'test_case': 'Test.2',
      },
    ]
    actual = process_output(data, ['Test.1', 'Test.2'], 10.0, 0)
    self.assertEquals(expected, actual)

  def test_process_output_missing(self):
    data = (
      '[ RUN      ] Test.2\n'
      '[       OK ] Test.2 (2000 ms)\n')
    expected = [
      {
        'duration': 23.,
        'output': '[ RUN      ] Test.2\n[       OK ] Test.2 (2000 ms)\n',
        'returncode': 0,
        'test_case': 'Test.2',
      },
      {
        'duration': None,
        'output': None,
        'returncode': None,
        'test_case': 'Test.1',
      },
    ]
    actual = process_output(data, ['Test.1', 'Test.2'], 23.0, 0)
    self.assertEquals(expected, actual)

  def test_calc_cluster_default(self):
    expected = [
      ((0, 1), 0),
      ((1, 1), 1),
      ((1, 10), 1),
      ((10, 10), 1),
      ((10, 100), 1),

      # Most VMs have 4 or 8 CPUs, asserts the values are sane.
      ((5, 1), 5),
      ((5, 2), 2),
      ((5, 4), 1),
      ((5, 8), 1),
      ((10, 1), 2),
      ((10, 4), 2),
      ((10, 8), 1),
      ((100, 1), 10),
      ((100, 4), 5),
      ((100, 8), 3),
      ((1000, 1), 10),
      ((1000, 4), 10),
      ((1000, 8), 10),
      ((3000, 1), 10),
      ((3000, 4), 10),
      ((3000, 8), 10),
    ]
    actual = [
      ((num_test_cases, jobs),
        run_test_cases.calc_cluster_default(num_test_cases, jobs))
      for (num_test_cases, jobs), _ in expected
    ]
    self.assertEquals(expected, actual)


class RunTestCasesTmp(unittest.TestCase):
  def setUp(self):
    super(RunTestCasesTmp, self).setUp()
    self.tmpdir = tempfile.mkdtemp(prefix='run_test_cases')

  def tearDown(self):
    shutil.rmtree(self.tmpdir)
    super(RunTestCasesTmp, self).tearDown()

  def test_xml(self):
    # Test when a file is already present that the index is increasing
    # accordingly.
    open(os.path.join(self.tmpdir, 'a.xml'), 'w').close()
    open(os.path.join(self.tmpdir, 'a_0.xml'), 'w').close()
    open(os.path.join(self.tmpdir, 'a_1.xml'), 'w').close()
    self.assertEqual(
        os.path.join(self.tmpdir, 'a_2.xml'),
        run_test_cases.gen_gtest_output_dir(self.tmpdir, 'xml:a.xml'))

  def test_xml_default(self):
    self.assertEqual(
        os.path.join(self.tmpdir, 'test_detail.xml'),
        run_test_cases.gen_gtest_output_dir(self.tmpdir, 'xml'))

  def test_gen_xml(self):
    data = {
      "duration": 7.895771026611328,
      "expected": 500,
      "fail": [
        "SecurityTest.MemoryAllocationRestrictionsCalloc",
        "SecurityTest.MemoryAllocationRestrictionsNewArray"
      ],
      "flaky": [
        "AlignedMemoryTest.DynamicAllocation",
        "AlignedMemoryTest.ScopedDynamicAllocation",
      ],
      "missing": [
        "AlignedMemoryTest.DynamicAllocation",
        "AlignedMemoryTest.ScopedDynamicAllocation",
      ],
      "success": [
        "AtExitTest.Param",
        "AtExitTest.Task",
      ],
      "test_cases": {
        "AlignedMemoryTest.DynamicAllocation": [
          {
            "duration": 0.044817209243774414,
            "output": "blah blah",
            "returncode": 1,
            "test_case": "AlignedMemoryTest.DynamicAllocation"
          }
        ],
        "AlignedMemoryTest.ScopedDynamicAllocation": [
          {
            "duration": 0.03273797035217285,
            "output": "blah blah",
            "returncode": 0,
            "test_case": "AlignedMemoryTest.ScopedDynamicAllocation"
          }
        ],
      },
    }
    expected = (
        '<?xml version="1.0" ?>\n'
        '<testsuites name="AllTests" tests="500" time="7.895771" '
          'timestamp="1996">\n'
        '<testsuite name="AlignedMemoryTest" tests="2">\n'
        '  <testcase classname="AlignedMemoryTest" name="DynamicAllocation" '
          'status="run" time="0.044817">\n'
          '<failure><![CDATA[blah blah]]></failure></testcase>\n'
        '  <testcase classname="AlignedMemoryTest" '
          'name="ScopedDynamicAllocation" status="run" time="0.032738"/>\n'
        '</testsuite>\n'
        '</testsuites>')
    filepath = os.path.join(self.tmpdir, 'foo.xml')
    run_test_cases.dump_results_as_xml(filepath, data, '1996')
    with open(filepath, 'rb') as f:
      actual = f.read()
    self.assertEqual(expected, actual)


class FakeProgress(object):
  @staticmethod
  def print_update():
    pass


class WorkerPoolTest(unittest.TestCase):
  def test_normal(self):
    mapper = lambda value: -value
    progress = FakeProgress()
    with run_test_cases.ThreadPool(progress, 8, 8, 0) as pool:
      for i in range(32):
        pool.add_task(0, mapper, i)
      results = pool.join()
    self.assertEquals(range(-31, 1), sorted(results))

  def test_exception(self):
    class FearsomeException(Exception):
      pass
    def mapper(value):
      raise FearsomeException(value)
    task_added = False
    try:
      progress = FakeProgress()
      with run_test_cases.ThreadPool(progress, 8, 8, 0) as pool:
        pool.add_task(0, mapper, 0)
        task_added = True
        pool.join()
        self.fail()
    except FearsomeException:
      self.assertEquals(True, task_added)


if __name__ == '__main__':
  VERBOSE = '-v' in sys.argv
  logging.basicConfig(level=logging.DEBUG if VERBOSE else logging.ERROR)
  unittest.TestCase.maxDiff = 5000
  unittest.main()
