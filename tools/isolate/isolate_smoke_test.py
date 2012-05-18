#!/usr/bin/env python
# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import cStringIO
import hashlib
import json
import logging
import os
import re
import shutil
import subprocess
import sys
import tempfile
import unittest

import isolate

ROOT_DIR = os.path.dirname(os.path.abspath(__file__))
VERBOSE = False


# Keep the list hard coded.
EXPECTED_MODES = ('check', 'hashtable', 'remap', 'run', 'trace')
# These are per test case, not per mode.
RELATIVE_CWD = {
  'fail': '.',
  'missing_trailing_slash': '.',
  'no_run': '.',
  'non_existent': '.',
  'touch_root': os.path.join('data', 'isolate'),
  'with_flag': '.',
}
DEPENDENCIES = {
  'fail': ['fail.py'],
  'missing_trailing_slash': [],
  'no_run': [
    'no_run.isolate',
    os.path.join('files1', 'test_file1.txt'),
    os.path.join('files1', 'test_file2.txt'),
  ],
  'non_existent': [],
  'touch_root': [
    os.path.join('data', 'isolate', 'touch_root.py'),
    'isolate.py',
  ],
  'with_flag': [
    'with_flag.py',
    os.path.join('files1', 'test_file1.txt'),
    os.path.join('files1', 'test_file2.txt'),
  ],
}

class CalledProcessError(subprocess.CalledProcessError):
  """Makes 2.6 version act like 2.7"""
  def __init__(self, returncode, cmd, output, cwd):
    super(CalledProcessError, self).__init__(returncode, cmd)
    self.output = output
    self.cwd = cwd

  def __str__(self):
    return super(CalledProcessError, self).__str__() + (
        '\n'
        'cwd=%s\n%s') % (self.cwd, self.output)


def list_files_tree(directory):
  """Returns the list of all the files in a tree."""
  actual = []
  for root, _dirs, files in os.walk(directory):
    actual.extend(os.path.join(root, f)[len(directory)+1:] for f in files)
  return sorted(actual)


def calc_sha1(filepath):
  """Calculates the SHA-1 hash for a file."""
  return hashlib.sha1(open(filepath, 'rb').read()).hexdigest()


class IsolateBase(unittest.TestCase):
  # To be defined by the subclass, it defines the amount of meta data saved by
  # isolate.py for each file. Should be one of (NO_INFO, STATS_ONLY, WITH_HASH).
  LEVEL = None

  def setUp(self):
    # The tests assume the current directory is the file's directory.
    os.chdir(ROOT_DIR)
    self.tempdir = tempfile.mkdtemp()
    self.result = os.path.join(self.tempdir, 'isolate_smoke_test.results')
    self.outdir = os.path.join(self.tempdir, 'isolated')

  def tearDown(self):
    shutil.rmtree(self.tempdir)

  def _expect_no_tree(self):
    self.assertFalse(os.path.exists(self.outdir))

  def _result_tree(self):
    return list_files_tree(self.outdir)

  def _expected_tree(self):
    """Verifies the files written in the temporary directory."""
    self.assertEquals(sorted(DEPENDENCIES[self.case()]), self._result_tree())

  @staticmethod
  def _fix_file_mode(filename, read_only):
    """4 modes are supported, 0755 (rwx), 0644 (rw), 0555 (rx), 0444 (r)."""
    min_mode = 0444
    if not read_only:
      min_mode |= 0200
    return (min_mode | 0111) if filename.endswith('.py') else min_mode

  def _gen_files(self, read_only):
    root_dir = ROOT_DIR
    if RELATIVE_CWD[self.case()] == '.':
      root_dir = os.path.join(root_dir, 'data', 'isolate')

    files = dict((unicode(f), {}) for f in DEPENDENCIES[self.case()])

    if self.LEVEL >= isolate.STATS_ONLY:
      for k, v in files.iteritems():
        if isolate.trace_inputs.get_flavor() != 'win':
          v[u'mode'] = self._fix_file_mode(k, read_only)
        filestats = os.stat(os.path.join(root_dir, k))
        v[u'size'] = filestats.st_size
        # Used the skip recalculating the hash. Use the most recent update
        # time.
        v[u'timestamp'] = int(round(filestats.st_mtime))

    if self.LEVEL >= isolate.WITH_HASH:
      for filename in files:
        files[filename][u'sha-1'] = unicode(
            calc_sha1(os.path.join(root_dir, filename)))
    return files

  def _expected_result(self, args, read_only):
    """Verifies self.result contains the expected data."""
    expected = {
      u'files': self._gen_files(read_only),
      u'read_only': read_only,
      u'relative_cwd': unicode(RELATIVE_CWD[self.case()]),
    }
    if args:
      expected[u'command'] = [u'python'] + [unicode(x) for x in args]
    else:
      expected[u'command'] = []
    self.assertEquals(expected, json.load(open(self.result, 'r')))

  def _expected_saved_state(self, extra_vars):
    flavor = isolate.trace_inputs.get_flavor()
    expected = {
      u'isolate_file': unicode(self.filename()),
      u'variables': {
        u'EXECUTABLE_SUFFIX': '.exe' if flavor == 'win' else '',
        u'OS': unicode(flavor),
      },
    }
    expected['variables'].update(extra_vars or {})
    self.assertEquals(expected, json.load(open(self.saved_state(), 'r')))

  def _expect_results(self, args, read_only, extra_vars):
    self._expected_result(args, read_only)
    self._expected_saved_state(extra_vars)

  def _expect_no_result(self):
    self.assertFalse(os.path.exists(self.result))

  def _execute(self, mode, case, args, need_output):
    """Executes isolate.py."""
    self.assertEquals(
        mode, self.mode(), 'Rename the test fixture to Isolate_%s' % mode)
    self.assertEquals(
        case,
        self.case() + '.isolate',
        'Rename the test case to test_%s()' % case)
    cmd = [
      sys.executable, os.path.join(ROOT_DIR, 'isolate.py'),
      '--result', self.result,
      '--outdir', self.outdir,
      self.filename(),
      '--mode', self.mode(),
    ]
    cmd.extend(args)

    env = os.environ.copy()
    if 'ISOLATE_DEBUG' in env:
      del env['ISOLATE_DEBUG']

    if need_output or not VERBOSE:
      stdout = subprocess.PIPE
      stderr = subprocess.STDOUT
    else:
      cmd.extend(['-v'] * 3)
      stdout = None
      stderr = None

    cwd = ROOT_DIR
    p = subprocess.Popen(
        cmd,
        stdout=stdout,
        stderr=stderr,
        cwd=cwd,
        env=env,
        universal_newlines=True)
    out = p.communicate()[0]
    if p.returncode:
      raise CalledProcessError(p.returncode, cmd, out, cwd)
    return out

  def mode(self):
    """Returns the execution mode corresponding to this test case."""
    test_id = self.id().split('.')
    self.assertEquals(3, len(test_id))
    self.assertEquals('__main__', test_id[0])
    return re.match('^Isolate_([a-z]+)$', test_id[1]).group(1)

  def case(self):
    """Returns the filename corresponding to this test case."""
    test_id = self.id().split('.')
    return re.match('^test_([a-z_]+)$', test_id[2]).group(1)

  def filename(self):
    """Returns the filename corresponding to this test case."""
    filename = os.path.join(
        ROOT_DIR, 'data', 'isolate', self.case() + '.isolate')
    self.assertTrue(os.path.isfile(filename), filename)
    return filename

  def saved_state(self):
    return isolate.result_to_state(self.result)


class Isolate(unittest.TestCase):
  def test_help_modes(self):
    # Check coherency in the help and implemented modes.
    p = subprocess.Popen(
        [sys.executable, os.path.join(ROOT_DIR, 'isolate.py'), '--help'],
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
        cwd=ROOT_DIR)
    out = p.communicate()[0].splitlines()
    self.assertEquals(0, p.returncode)
    out = out[out.index('') + 1:]
    out = out[:out.index('')]
    modes = [re.match(r'^  (\w+) .+', l) for l in out]
    modes = tuple(m.group(1) for m in modes if m)
    # noop doesn't do anything so no point in testing it.
    self.assertEquals(sorted(EXPECTED_MODES + ('noop',)), sorted(modes))

  def test_modes(self):
    # This is a bit redundant but make sure all combinations are tested.
    files = sorted(
      i[:-len('.isolate')]
      for i in os.listdir(os.path.join(ROOT_DIR, 'data', 'isolate'))
      if i.endswith('.isolate')
    )
    self.assertEquals(sorted(RELATIVE_CWD), files)
    self.assertEquals(sorted(DEPENDENCIES), files)
    for mode in EXPECTED_MODES:
      expected_cases = set('test_%s' % f for f in files)
      fixture_name = 'Isolate_%s' % mode
      fixture = getattr(sys.modules[__name__], fixture_name)
      actual_cases = set(i for i in dir(fixture) if i.startswith('test_'))
      missing = expected_cases - actual_cases
      self.assertFalse(missing, '%s.%s' % (fixture_name, missing))


class Isolate_check(IsolateBase):
  LEVEL = isolate.NO_INFO

  def test_fail(self):
    self._execute('check', 'fail.isolate', [], False)
    self._expect_no_tree()
    self._expect_results(['fail.py'], None, None)

  def test_missing_trailing_slash(self):
    try:
      self._execute('check', 'missing_trailing_slash.isolate', [], False)
      self.fail()
    except subprocess.CalledProcessError:
      pass
    self._expect_no_tree()
    self._expect_no_result()

  def test_non_existent(self):
    try:
      self._execute('check', 'non_existent.isolate', [], False)
      self.fail()
    except subprocess.CalledProcessError:
      pass
    self._expect_no_tree()
    self._expect_no_result()

  def test_no_run(self):
    self._execute('check', 'no_run.isolate', [], False)
    self._expect_no_tree()
    self._expect_results([], None, None)

  def test_touch_root(self):
    self._execute('check', 'touch_root.isolate', [], False)
    self._expect_no_tree()
    self._expect_results(['touch_root.py'], None, None)

  def test_with_flag(self):
    self._execute('check', 'with_flag.isolate', ['-V', 'FLAG', 'gyp'], False)
    self._expect_no_tree()
    self._expect_results(
        ['with_flag.py', 'gyp'], None, {u'FLAG': u'gyp'})


class Isolate_hashtable(IsolateBase):
  LEVEL = isolate.WITH_HASH

  def _expected_hash_tree(self):
    """Verifies the files written in the temporary directory."""
    expected = [v['sha-1'] for v in self._gen_files(False).itervalues()]
    self.assertEquals(sorted(expected), self._result_tree())

  def test_fail(self):
    self._execute('hashtable', 'fail.isolate', [], False)
    self._expected_hash_tree()
    self._expect_results(['fail.py'], None, None)

  def test_missing_trailing_slash(self):
    try:
      self._execute('hashtable', 'missing_trailing_slash.isolate', [], False)
      self.fail()
    except subprocess.CalledProcessError:
      pass
    self._expect_no_tree()
    self._expect_no_result()

  def test_non_existent(self):
    try:
      self._execute('hashtable', 'non_existent.isolate', [], False)
      self.fail()
    except subprocess.CalledProcessError:
      pass
    self._expect_no_tree()
    self._expect_no_result()

  def test_no_run(self):
    self._execute('hashtable', 'no_run.isolate', [], False)
    self._expected_hash_tree()
    self._expect_results([], None, None)

  def test_touch_root(self):
    self._execute('hashtable', 'touch_root.isolate', [], False)
    self._expected_hash_tree()
    self._expect_results(['touch_root.py'], None, None)

  def test_with_flag(self):
    self._execute(
        'hashtable', 'with_flag.isolate', ['-V', 'FLAG', 'gyp'], False)
    self._expected_hash_tree()
    self._expect_results(
        ['with_flag.py', 'gyp'], None, {u'FLAG': u'gyp'})


class Isolate_remap(IsolateBase):
  LEVEL = isolate.STATS_ONLY

  def test_fail(self):
    self._execute('remap', 'fail.isolate', [], False)
    self._expected_tree()
    self._expect_results(['fail.py'], None, None)

  def test_missing_trailing_slash(self):
    try:
      self._execute('remap', 'missing_trailing_slash.isolate', [], False)
      self.fail()
    except subprocess.CalledProcessError:
      pass
    self._expect_no_tree()
    self._expect_no_result()

  def test_non_existent(self):
    try:
      self._execute('remap', 'non_existent.isolate', [], False)
      self.fail()
    except subprocess.CalledProcessError:
      pass
    self._expect_no_tree()
    self._expect_no_result()

  def test_no_run(self):
    self._execute('remap', 'no_run.isolate', [], False)
    self._expected_tree()
    self._expect_results([], None, None)

  def test_touch_root(self):
    self._execute('remap', 'touch_root.isolate', [], False)
    self._expected_tree()
    self._expect_results(['touch_root.py'], None, None)

  def test_with_flag(self):
    self._execute('remap', 'with_flag.isolate', ['-V', 'FLAG', 'gyp'], False)
    self._expected_tree()
    self._expect_results(
        ['with_flag.py', 'gyp'], None, {u'FLAG': u'gyp'})


class Isolate_run(IsolateBase):
  LEVEL = isolate.STATS_ONLY

  def _expect_empty_tree(self):
    self.assertEquals([], self._result_tree())

  def test_fail(self):
    try:
      self._execute('run', 'fail.isolate', [], False)
      self.fail()
    except subprocess.CalledProcessError:
      pass
    self._expect_empty_tree()
    self._expect_results(['fail.py'], None, None)

  def test_missing_trailing_slash(self):
    try:
      self._execute('run', 'missing_trailing_slash.isolate', [], False)
      self.fail()
    except subprocess.CalledProcessError:
      pass
    self._expect_no_tree()
    self._expect_no_result()

  def test_non_existent(self):
    try:
      self._execute('run', 'non_existent.isolate', [], False)
      self.fail()
    except subprocess.CalledProcessError:
      pass
    self._expect_no_tree()
    self._expect_no_result()

  def test_no_run(self):
    try:
      self._execute('run', 'no_run.isolate', [], False)
      self.fail()
    except subprocess.CalledProcessError:
      pass
    self._expect_empty_tree()
    self._expect_results([], None, None)

  def test_touch_root(self):
    self._execute('run', 'touch_root.isolate', [], False)
    self._expect_empty_tree()
    self._expect_results(['touch_root.py'], None, None)

  def test_with_flag(self):
    self._execute('run', 'with_flag.isolate', ['-V', 'FLAG', 'run'], False)
    # Not sure about the empty tree, should be deleted.
    self._expect_empty_tree()
    self._expect_results(
        ['with_flag.py', 'run'], None, {u'FLAG': u'run'})


class Isolate_trace(IsolateBase):
  LEVEL = isolate.STATS_ONLY

  @staticmethod
  def _to_string(values):
    buf = cStringIO.StringIO()
    isolate.trace_inputs.pretty_print(values, buf)
    return buf.getvalue()

  def test_fail(self):
    try:
      self._execute('trace', 'fail.isolate', ['-v'], True)
      self.fail()
    except subprocess.CalledProcessError, e:
      out = e.output
    self._expect_no_tree()
    self._expect_results(['fail.py'], None, None)
    expected = 'Failing'
    if sys.platform == 'win32':
      # Includes spew from tracerpt.exe.
      self.assertTrue(out.startswith(expected))
    else:
      self.assertEquals(expected, out.rstrip())

  def test_missing_trailing_slash(self):
    try:
      self._execute('trace', 'missing_trailing_slash.isolate', [], True)
      self.fail()
    except subprocess.CalledProcessError, e:
      out = e.output
    self._expect_no_tree()
    self._expect_no_result()
    expected = (
      'Usage: isolate.py [options] [.isolate file]\n\n'
      'isolate.py: error: Input directory %s must have a trailing slash\n' %
          os.path.join(ROOT_DIR, 'data', 'isolate', 'files1')
    )
    self.assertEquals(expected, out)

  def test_non_existent(self):
    try:
      self._execute('trace', 'non_existent.isolate', [], True)
      self.fail()
    except subprocess.CalledProcessError, e:
      out = e.output
    self._expect_no_tree()
    self._expect_no_result()
    expected = (
      'Usage: isolate.py [options] [.isolate file]\n\n'
      'isolate.py: error: Input file %s doesn\'t exist\n' %
          os.path.join(ROOT_DIR, 'data', 'isolate', 'A_file_that_do_not_exist')
    )
    self.assertEquals(expected, out)

  def test_no_run(self):
    try:
      self._execute('trace', 'no_run.isolate', [], True)
      self.fail()
    except subprocess.CalledProcessError, e:
      out = e.output
    self._expect_no_tree()
    self._expect_results([], None, None)
    expected = 'No command to run\n'
    self.assertEquals(expected, out)

  def test_touch_root(self):
    out = self._execute('trace', 'touch_root.isolate', [], True)
    self._expect_no_tree()
    self._expect_results(['touch_root.py'], None, None)
    expected = {
      'conditions': [
        ['OS=="%s"' % isolate.trace_inputs.get_flavor(), {
          'variables': {
            isolate.trace_inputs.KEY_TRACKED: [
              'touch_root.py',
              '../../isolate.py',
            ],
          },
        }],
      ],
    }
    self.assertEquals(self._to_string(expected), out)

  def test_with_flag(self):
    out = self._execute(
        'trace', 'with_flag.isolate', ['-V', 'FLAG', 'trace'], True)
    self._expect_no_tree()
    self._expect_results(['with_flag.py', 'trace'], None, {u'FLAG': u'trace'})
    expected = {
      'conditions': [
        ['OS=="%s"' % isolate.trace_inputs.get_flavor(), {
          'variables': {
            isolate.trace_inputs.KEY_TRACKED: [
              'with_flag.py',
            ],
            isolate.trace_inputs.KEY_UNTRACKED: [
              # Note that .isolate format mandates / and not os.path.sep.
              'files1/',
            ],
          },
        }],
      ],
    }
    self.assertEquals(self._to_string(expected), out)


class IsolateNoOutdir(IsolateBase):
  # Test without the --outdir flag.
  # So all the files are first copied in the tempdir and the test is run from
  # there.
  def setUp(self):
    super(IsolateNoOutdir, self).setUp()
    self.root = os.path.join(self.tempdir, 'root')
    os.makedirs(os.path.join(self.root, 'data', 'isolate'))
    for i in ('touch_root.isolate', 'touch_root.py'):
      shutil.copy(
          os.path.join(ROOT_DIR, 'data', 'isolate', i),
          os.path.join(self.root, 'data', 'isolate', i))
      shutil.copy(
          os.path.join(ROOT_DIR, 'isolate.py'),
          os.path.join(self.root, 'isolate.py'))

  def _execute(self, mode, case, args, need_output):
    """Executes isolate.py."""
    self.assertEquals(
        mode, self.mode(), 'Rename the test fixture to Isolate_%s' % mode)
    self.assertEquals(
        case,
        self.case() + '.isolate',
        'Rename the test case to test_%s()' % case)
    cmd = [
      sys.executable, os.path.join(ROOT_DIR, 'isolate.py'),
      '--result', self.result,
      self.filename(),
      '--mode', self.mode(),
    ]
    cmd.extend(args)

    env = os.environ.copy()
    if 'ISOLATE_DEBUG' in env:
      del env['ISOLATE_DEBUG']

    if need_output or not VERBOSE:
      stdout = subprocess.PIPE
      stderr = subprocess.STDOUT
    else:
      cmd.extend(['-v'] * 3)
      stdout = None
      stderr = None

    cwd = self.tempdir
    p = subprocess.Popen(
        cmd,
        stdout=stdout,
        stderr=stderr,
        cwd=cwd,
        env=env,
        universal_newlines=True)
    out = p.communicate()[0]
    if p.returncode:
      raise CalledProcessError(p.returncode, cmd, out, cwd)
    return out

  def mode(self):
    """Returns the execution mode corresponding to this test case."""
    test_id = self.id().split('.')
    self.assertEquals(3, len(test_id))
    self.assertEquals('__main__', test_id[0])
    return re.match('^test_([a-z]+)$', test_id[2]).group(1)

  def case(self):
    """Returns the filename corresponding to this test case."""
    return 'touch_root'

  def filename(self):
    """Returns the filename corresponding to this test case."""
    filename = os.path.join(
        self.root, 'data', 'isolate', self.case() + '.isolate')
    self.assertTrue(os.path.isfile(filename), filename)
    return filename

  def test_check(self):
    self._execute('check', 'touch_root.isolate', [], False)
    files = [
      'isolate_smoke_test.results',
      'isolate_smoke_test.state',
      os.path.join('root', 'data', 'isolate', 'touch_root.isolate'),
      os.path.join('root', 'data', 'isolate', 'touch_root.py'),
      os.path.join('root', 'isolate.py'),
    ]
    self.assertEquals(files, list_files_tree(self.tempdir))

  def test_hashtable(self):
    self._execute('hashtable', 'touch_root.isolate', [], False)
    files = [
      os.path.join(
          'hashtable', calc_sha1(os.path.join(ROOT_DIR, 'isolate.py'))),
      os.path.join(
          'hashtable',
          calc_sha1(
              os.path.join(ROOT_DIR, 'data', 'isolate', 'touch_root.py'))),
      'isolate_smoke_test.results',
      'isolate_smoke_test.state',
      os.path.join('root', 'data', 'isolate', 'touch_root.isolate'),
      os.path.join('root', 'data', 'isolate', 'touch_root.py'),
      os.path.join('root', 'isolate.py'),
    ]
    self.assertEquals(files, list_files_tree(self.tempdir))

  def test_remap(self):
    self._execute('remap', 'touch_root.isolate', [], False)
    files = [
      'isolate_smoke_test.results',
      'isolate_smoke_test.state',
      os.path.join('root', 'data', 'isolate', 'touch_root.isolate'),
      os.path.join('root', 'data', 'isolate', 'touch_root.py'),
      os.path.join('root', 'isolate.py'),
    ]
    self.assertEquals(files, list_files_tree(self.tempdir))

  def test_run(self):
    self._execute('run', 'touch_root.isolate', [], False)
    files = [
      'isolate_smoke_test.results',
      'isolate_smoke_test.state',
      os.path.join('root', 'data', 'isolate', 'touch_root.isolate'),
      os.path.join('root', 'data', 'isolate', 'touch_root.py'),
      os.path.join('root', 'isolate.py'),
    ]
    self.assertEquals(files, list_files_tree(self.tempdir))

  def test_trace(self):
    self._execute('trace', 'touch_root.isolate', [], False)
    files = [
      'isolate_smoke_test.results',
      'isolate_smoke_test.state',
      os.path.join('root', 'data', 'isolate', 'touch_root.isolate'),
      os.path.join('root', 'data', 'isolate', 'touch_root.py'),
      os.path.join('root', 'isolate.py'),
    ]
    # Clean the directory from the logs, which are OS-specific.
    isolate.trace_inputs.get_api().clean_trace(
        os.path.join(self.tempdir, 'isolate_smoke_test.results.log'))
    self.assertEquals(files, list_files_tree(self.tempdir))


if __name__ == '__main__':
  VERBOSE = '-v' in sys.argv
  logging.basicConfig(level=logging.DEBUG if VERBOSE else logging.ERROR)
  unittest.main()
