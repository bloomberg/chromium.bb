#!/usr/bin/env python
# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import json
import logging
import os
import re
import shutil
import subprocess
import sys
import tempfile
import unittest
from xml.dom import minidom

ROOT_DIR = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, ROOT_DIR)
sys.path.append(os.path.join(ROOT_DIR, 'tests', 'gtest_fake'))

import gtest_fake_base
import run_test_cases


def RunTest(arguments):
  cmd = [
      sys.executable,
      os.path.join(ROOT_DIR, 'run_test_cases.py'),
  ] + arguments

  logging.debug(' '.join(cmd))
  # Do not use universal_newline=True since run_test_cases uses CR extensively.
  proc = subprocess.Popen(
      cmd,
      stdout=subprocess.PIPE,
      stderr=subprocess.PIPE)

  # pylint is confused.
  out, err = proc.communicate() or ('', '')
  if sys.platform == 'win32':
    # Downgrade CRLF to LF.
    out = out.replace('\r\n', '\n')
    err = err.replace('\r\n', '\n')
  else:
    # Upgrade CR to LF.
    out = out.replace('\r', '\n')
    err = err.replace('\r', '\n')

  return (out, err, proc.returncode)


def trim_xml_whitespace(data):
  """Recursively remove non-important elements."""
  children_to_remove = []
  for child in data.childNodes:
    if child.nodeType == minidom.Node.TEXT_NODE and not child.data.strip():
      children_to_remove.append(child)
    elif child.nodeType == minidom.Node.COMMENT_NODE:
      children_to_remove.append(child)
    elif child.nodeType == minidom.Node.ELEMENT_NODE:
      trim_xml_whitespace(child)
  for child in children_to_remove:
    data.removeChild(child)


def load_xml_as_string_and_filter(filepath):
  """Serializes XML to a list of strings that is consistently formatted
  (ignoring whitespace between elements) so that it may be compared.
  """
  with open(filepath, 'rb') as f:
    xml = minidom.parse(f)

  trim_xml_whitespace(xml)

  # Very hardcoded for expected.xml.
  xml.childNodes[0].attributes['time'] = "0.1"
  xml.childNodes[0].attributes['timestamp'] = "1996"
  xml.childNodes[0].childNodes[0].childNodes[0].attributes['time'] = "0.2"
  xml.childNodes[0].childNodes[0].childNodes[1].attributes['time'] = "0.2"
  xml.childNodes[0].childNodes[0].childNodes[2].attributes['time'] = "0.2"
  return xml.toprettyxml(indent='  ').splitlines()


class RunTestCases(unittest.TestCase):
  def setUp(self):
    super(RunTestCases, self).setUp()
    # Make sure there's no environment variable that could do side effects.
    os.environ.pop('GTEST_SHARD_INDEX', '')
    os.environ.pop('GTEST_TOTAL_SHARDS', '')
    self._tempdirpath = None
    self._tempfilename = None

  def tearDown(self):
    if self._tempdirpath and os.path.exists(self._tempdirpath):
      shutil.rmtree(self._tempdirpath)
    if self._tempfilename and os.path.exists(self._tempfilename):
      os.remove(self._tempfilename)
    super(RunTestCases, self).tearDown()

  @property
  def tempdirpath(self):
    if not self._tempdirpath:
      self._tempdirpath = tempfile.mkdtemp(prefix='run_test_cases')
    return self._tempdirpath

  @property
  def filename(self):
    if not self._tempfilename:
      handle, self._tempfilename = tempfile.mkstemp(
          prefix='run_test_cases', suffix='.run_test_cases')
      os.close(handle)
    return self._tempfilename

  def _check_results(self, expected_out_re, out, err):
    lines = out.splitlines()

    for index in range(len(expected_out_re)):
      if not lines:
        self.fail('%s\nerr:\n%s' % ('\n'.join(expected_out_re[index:]), err))
      line = lines.pop(0)
      if not re.match('^%s$' % expected_out_re[index], line):
        self.fail(
          '\nIndex: %d\nExpected: %r\nLine: %r\nNext lines: %s' % (
           index,
           expected_out_re[index],
           line,
           '\n'.join(lines[index:index+5])))
    self.assertEqual([], lines)
    self.assertEqual('', err)

  def _check_results_file(self, fail, flaky, success, test_cases, duration):
    self.assertTrue(os.path.exists(self.filename))

    with open(self.filename) as f:
      actual = json.load(f)

    self.assertEqual(
        [
          u'duration', u'expected', u'fail', u'flaky', u'missing', u'success',
          u'test_cases',
        ],
        sorted(actual))

    if duration:
      self.assertTrue(actual['duration'] > 0.0000001)
    else:
      self.assertEqual(actual['duration'], 0)
    self.assertEqual(fail, actual['fail'])
    self.assertEqual(flaky, actual['flaky'])
    self.assertEqual(success, actual['success'])
    self.assertEqual(len(test_cases), len(actual['test_cases']))
    for (entry_name, entry_count) in test_cases:
      self.assertTrue(entry_name in actual['test_cases'])
      self.assertEqual(
          entry_count,
          len(actual['test_cases'][entry_name]),
          (entry_count, len(actual['test_cases'][entry_name]), entry_name))

  def test_simple_pass(self):
    out, err, return_code = RunTest(
        [
          '--clusters', '1',
          '--jobs', '3',
          '--result', self.filename,
          os.path.join(ROOT_DIR, 'tests', 'gtest_fake', 'gtest_fake_pass.py'),
        ])

    self.assertEqual(0, return_code)

    expected_out_re = [
      r'\[\d/\d\]   \d\.\d\ds .+',
      r'\[\d/\d\]   \d\.\d\ds .+',
      r'\[\d/\d\]   \d\.\d\ds .+',
      re.escape('Summary:'),
      re.escape('  Success:    3 100.00% ') + r' +\d+\.\d\ds',
      re.escape('    Flaky:    0   0.00% ') + r' +\d+\.\d\ds',
      re.escape('     Fail:    0   0.00% ') + r' +\d+\.\d\ds',
      r'  \d+\.\d\ds Done running 3 tests with 3 executions. \d+\.\d\d test/s',
    ]
    self._check_results(expected_out_re, out, err)

    test_cases = [
        ('Foo.Bar1', 1),
        ('Foo.Bar2', 1),
        ('Foo.Bar/3', 1)
    ]
    self._check_results_file(
        fail=[],
        flaky=[],
        success=sorted([u'Foo.Bar1', u'Foo.Bar2', u'Foo.Bar/3']),
        test_cases=test_cases,
        duration=True)

  def test_simple_pass_cluster(self):
    out, err, return_code = RunTest(
        [
          '--clusters', '10',
          '--jobs', '1',
          '--result', self.filename,
          os.path.join(ROOT_DIR, 'tests', 'gtest_fake', 'gtest_fake_pass.py'),
        ])

    self.assertEqual(0, return_code)

    expected_out_re = [
      r'\[\d/\d\]   \d\.\d\ds .+',
      r'\[\d/\d\]   \d\.\d\ds .+',
      r'\[\d/\d\]   \d\.\d\ds .+',
      re.escape('Summary:'),
      re.escape('  Success:    3 100.00% ') + r' +\d+\.\d\ds',
      re.escape('    Flaky:    0   0.00% ') + r' +\d+\.\d\ds',
      re.escape('     Fail:    0   0.00% ') + r' +\d+\.\d\ds',
      r'  \d+\.\d\ds Done running 3 tests with 3 executions. \d+\.\d\d test/s',
    ]
    self._check_results(expected_out_re, out, err)

    test_cases = [
        ('Foo.Bar1', 1),
        ('Foo.Bar2', 1),
        ('Foo.Bar/3', 1)
    ]
    self._check_results_file(
        fail=[],
        flaky=[],
        success=sorted([u'Foo.Bar1', u'Foo.Bar2', u'Foo.Bar/3']),
        test_cases=test_cases,
        duration=True)

  def test_simple_pass_verbose(self):
    # We take verbosity seriously so test it.
    out, err, return_code = RunTest(
        [
          # Linearize execution.
          '--clusters', '1',
          '--jobs', '1',
          '--verbose',
          '--result', self.filename,
          os.path.join(ROOT_DIR, 'tests', 'gtest_fake', 'gtest_fake_pass.py'),
        ])

    self.assertEqual(0, return_code)

    expected_out_re = []
    test_cases = (
      'Foo.Bar2',
      'Foo.Bar1',
      'Foo.Bar/3',
    )

    for index, name in enumerate(test_cases):
      expected_out_re.append(
          r'\[%d/\d\]   \d\.\d\ds ' % (index + 1) + re.escape(name) + ' .+')
      expected_out_re.extend(
          re.escape(l) for l in
            gtest_fake_base.get_test_output_inner(name, False).splitlines())
      expected_out_re.append('')

    expected_out_re.extend([
      re.escape('Summary:'),
      re.escape('  Success:    3 100.00%') + r' +\d+\.\d\ds',
      re.escape('    Flaky:    0   0.00%') + r' +\d+\.\d\ds',
      re.escape('     Fail:    0   0.00%') + r' +\d+\.\d\ds',
      r'  \d+\.\d\ds Done running 3 tests with 3 executions. \d+\.\d\d test/s',
    ])
    self._check_results(expected_out_re, out, '')
    # Test 'err' manually.
    self.assertTrue(
        re.match(
            r'INFO  run_test_cases\(\d+\)\: Found 3 test cases in \S+ '
            r'\S+gtest_fake_pass.py',
            err.strip()),
        err)

  def test_simple_pass_very_verbose(self):
    # Ensure that the test still passes with maximum verbosity, but don't worry
    # about what the output is.
        # We take verbosity seriously so test it.
    _, err, return_code = RunTest(
        [
          # Linearize execution.
          '--clusters', '1',
          '--jobs', '1',
          '--verbose',
          '--verbose',
          '--verbose',
          '--result', self.filename,
          os.path.join(ROOT_DIR, 'tests', 'gtest_fake', 'gtest_fake_pass.py'),
        ])

    self.assertEqual(0, return_code, err)

  def test_simple_fail(self):
    out, err, return_code = RunTest(
        [
          # Linearize execution.
          '--clusters', '1',
          '--jobs', '1',
          '--result', self.filename,
          os.path.join(ROOT_DIR, 'tests', 'gtest_fake', 'gtest_fake_fail.py'),
        ])

    self.assertEqual(1, return_code)

    test_failure_output = [
      re.escape(l) for l in
      gtest_fake_base.get_test_output_inner('Baz.Fail', True).splitlines()
    ] + [
      '',
    ]

    expected_out_re = [
      r'\[1/\d\]   \d\.\d\ds .+',
      r'\[2/\d\]   \d\.\d\ds .+',
      r'\[3/\d\]   \d\.\d\ds .+',
      r'\[4/\d\]   \d\.\d\ds .+',
    ] + test_failure_output + [
      # Retries
      r'\[5/\d\]   \d\.\d\ds .+ retry \#1',
    ] + test_failure_output + [
        re.escape(l) for l in run_test_cases.running_serial_warning()
    ] + [
      r'\[6/\d\]   \d\.\d\ds .+ retry \#2',
    ] + test_failure_output + [
      re.escape('Failed tests:'),
      re.escape('  Baz.Fail'),
      re.escape('Summary:'),
      re.escape('  Success:    3  75.00%') + r' +\d+\.\d\ds',
      re.escape('    Flaky:    0   0.00%') + r' +\d+\.\d\ds',
      re.escape('     Fail:    1  25.00%') + r' +\d+\.\d\ds',
      r'  \d+\.\d\ds Done running 4 tests with 6 executions. \d+\.\d\d test/s',
    ]
    self._check_results(expected_out_re, out, err)

    test_cases = [
        ('Foo.Bar1', 1),
        ('Foo.Bar2', 1),
        ('Foo.Bar3', 1),
        ('Baz.Fail', 3)
    ]
    self._check_results_file(
        fail=['Baz.Fail'],
        flaky=[],
        success=[u'Foo.Bar1', u'Foo.Bar2', u'Foo.Bar3'],
        test_cases=test_cases,
        duration=True)

  def test_simple_fail_verbose(self):
    # We take verbosity seriously so test it.
    out, err, return_code = RunTest(
        [
          # Linearize execution.
          '--clusters', '1',
          '--jobs', '1',
          '--verbose',
          '--result', self.filename,
          os.path.join(ROOT_DIR, 'tests', 'gtest_fake', 'gtest_fake_fail.py'),
        ])

    self.assertEqual(1, return_code)

    expected_out_re = []
    test_cases = (
      'Foo.Bar3',
      'Foo.Bar1',
      'Foo.Bar2',
      'Baz.Fail',
      'Baz.Fail',
      'Baz.Fail',
    )

    for index, name in enumerate(test_cases):
      if index + 1 == len(test_cases):
        # We are about to retry the test serially, so check for the warning.
        expected_out_re.extend(
            re.escape(l) for l in run_test_cases.running_serial_warning())

      expected_out_re.append(
          r'\[%d/\d\]   \d\.\d\ds ' % (index + 1) + re.escape(name) + ' .+')
      expected_out_re.extend(
          re.escape(l)
          for l in gtest_fake_base.get_test_output_inner(
              name, 'Fail' in name).splitlines())
      expected_out_re.append('')

    expected_out_re.extend([
      re.escape('Failed tests:'),
      re.escape('  Baz.Fail'),
      re.escape('Summary:'),
      re.escape('  Success:    3  75.00%') + r' +\d+\.\d\ds',
      re.escape('    Flaky:    0   0.00%') + r' +\d+\.\d\ds',
      re.escape('     Fail:    1  25.00%') + r' +\d+\.\d\ds',
      r'  \d+\.\d\ds Done running 4 tests with 6 executions. \d+\.\d\d test/s',
    ])
    self._check_results(expected_out_re, out, '')

    # Test 'err' manually.
    self.assertTrue(
        re.match(
            r'INFO  run_test_cases\(\d+\)\: Found 4 test cases in \S+ '
            r'\S+gtest_fake_fail.py',
            err.strip()),
        err)
    test_cases = [
        ('Foo.Bar1', 1),
        ('Foo.Bar2', 1),
        ('Foo.Bar3', 1),
        ('Baz.Fail', 3)
    ]
    self._check_results_file(
        fail=['Baz.Fail'],
        flaky=[],
        success=[u'Foo.Bar1', u'Foo.Bar2', u'Foo.Bar3'],
        test_cases=test_cases,
        duration=True)

  def test_simple_gtest_list_error(self):
    out, err, return_code = RunTest(
        [
          '--no-dump',
          os.path.join(ROOT_DIR, 'tests', 'gtest_fake', 'gtest_fake_error.py'),
        ])

    expected_out_re = [
        'Failed to list test cases',
        'Failed to run %s %s --gtest_list_tests' % (
          re.escape(sys.executable),
          re.escape(
            os.path.join(
              ROOT_DIR, 'tests', 'gtest_fake', 'gtest_fake_error.py'))),
        'stdout:',
        '',
        'stderr:',
        'Unable to list tests'
    ]

    self.assertEqual(1, return_code)
    self._check_results(expected_out_re, out, err)

  def test_gtest_list_tests(self):
    out, err, return_code = RunTest(
        [
          '--gtest_list_tests',
          os.path.join(ROOT_DIR, 'tests', 'gtest_fake', 'gtest_fake_fail.py'),
        ])

    expected_out = (
      'Foo.\n  Bar1\n  Bar2\n  Bar3\nBaz.\n  Fail\n'
      '  YOU HAVE 2 tests with ignored failures (FAILS prefix)\n\n')
    self.assertEqual('', err)
    self.assertEqual(expected_out, out)
    self.assertEqual(0, return_code)

  def test_flaky_stop_early(self):
    # gtest_fake_flaky.py has Foo.Bar[1-9]. Each of the test fails once and
    # succeeds on the second pass.
    out, err, return_code = RunTest(
        [
          '--result', self.filename,
          # Linearize execution.
          '--clusters', '1',
          '--jobs', '1',
          '--retries', '1',
          '--max-failures', '2',
          os.path.join(
            ROOT_DIR, 'tests', 'gtest_fake', 'gtest_fake_flaky.py'),
          self.tempdirpath,
        ])
    self.assertEqual(1, return_code)
    # Give up on checking the stdout.
    self.assertTrue('STOPPED EARLY' in out, out)
    self.assertEqual('', err)
    # The order is determined by the test shuffling.
    test_cases = [
        ('Foo.Bar1', 1),
        ('Foo.Bar4', 1),
        ('Foo.Bar5', 1),
    ]
    self._check_results_file(
        fail=[u'Foo.Bar1', u'Foo.Bar4', u'Foo.Bar5'],
        flaky=[],
        success=[],
        test_cases=test_cases,
        duration=True)

  def test_flaky_stop_early_xml(self):
    # Create an unique filename and delete the file.
    os.remove(self.filename)
    _, err, return_code = RunTest(
        [
          # In that case, it's an XML file even if it has the wrong extension.
          '--gtest_output=xml:' + self.filename,
          '--no-dump',
          # Linearize execution.
          '--clusters', '1',
          '--jobs', '1',
          '--retries', '1',
          '--max-failures', '2',
          os.path.join(
            ROOT_DIR, 'tests', 'gtest_fake', 'gtest_fake_flaky.py'),
          self.tempdirpath,
        ])
    self.assertEqual(1, return_code)
    # Give up on checking the stdout.
    #self.assertTrue('STOPPED EARLY' in out, out)
    self.assertEqual('', err)
    try:
      actual_xml = load_xml_as_string_and_filter(self.filename)
    except Exception as e:
      print >> sys.stderr, e
      print >> sys.stderr, self.filename
      with open(self.filename, 'rb') as f:
        print >> sys.stderr, f.read()
      self.fail()
    expected_xml = load_xml_as_string_and_filter(
        os.path.join(ROOT_DIR, 'tests', 'gtest_fake', 'expected.xml'))
    self.assertEqual(expected_xml, actual_xml)

  def test_gtest_filter(self):
    out, err, return_code = RunTest(
        [
          '--gtest_filter=Foo.Bar1:Foo.Bar/*',
          '--result', self.filename,
          os.path.join(ROOT_DIR, 'tests', 'gtest_fake', 'gtest_fake_pass.py'),
        ])

    self.assertEqual(0, return_code, (out, err))

    expected_out_re = [
      r'\[\d/\d\]   \d\.\d\ds .+',
      r'\[\d/\d\]   \d\.\d\ds .+',
      re.escape('Summary:'),
      re.escape('  Success:    2 100.00% ') + r' +\d+\.\d\ds',
      re.escape('    Flaky:    0   0.00% ') + r' +\d+\.\d\ds',
      re.escape('     Fail:    0   0.00% ') + r' +\d+\.\d\ds',
      r'  \d+\.\d\ds Done running 2 tests with 2 executions. \d+\.\d\d test/s',
    ]
    self._check_results(expected_out_re, out, err)

    test_cases = [
        ('Foo.Bar1', 1),
        ('Foo.Bar/3', 1)
    ]
    self._check_results_file(
        fail=[],
        flaky=[],
        success=sorted([u'Foo.Bar1', u'Foo.Bar/3']),
        test_cases=test_cases,
        duration=True)

  def test_gtest_filter_missing(self):
    out, err, return_code = RunTest(
        [
          '--gtest_filter=Not.Present',
          '--result', self.filename,
          os.path.join(ROOT_DIR, 'tests', 'gtest_fake', 'gtest_fake_pass.py'),
        ])

    self.assertEqual(1, return_code, (out, err))
    expected_out_re = ['Found no test to run']
    self._check_results(expected_out_re, out, err)
    self._check_results_file(
        fail=[],
        flaky=[],
        success=[],
        test_cases=[],
        duration=False)


if __name__ == '__main__':
  VERBOSE = '-v' in sys.argv
  logging.basicConfig(level=logging.DEBUG if VERBOSE else logging.ERROR)
  unittest.main()
