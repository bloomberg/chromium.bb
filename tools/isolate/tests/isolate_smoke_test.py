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
import stat
import subprocess
import sys
import tempfile
import unittest

ROOT_DIR = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, ROOT_DIR)

import isolate
import run_test_from_archive

VERBOSE = False
SHA_1_NULL = hashlib.sha1().hexdigest()


# Keep the list hard coded.
EXPECTED_MODES = (
  'check',
  'hashtable',
  'help',
  'noop',
  'merge',
  'read',
  'remap',
  'run',
  'trace',
)

# These are per test case, not per mode.
RELATIVE_CWD = {
  'fail': '.',
  'missing_trailing_slash': '.',
  'no_run': '.',
  'non_existent': '.',
  'symlink_full': '.',
  'symlink_partial': '.',
  'touch_only': '.',
  'touch_root': os.path.join('tests', 'isolate'),
  'with_flag': '.',
}

DEPENDENCIES = {
  'fail': ['fail.py'],
  'missing_trailing_slash': [],
  'no_run': [
    'no_run.isolate',
    os.path.join('files1', 'subdir', '42.txt'),
    os.path.join('files1', 'test_file1.txt'),
    os.path.join('files1', 'test_file2.txt'),
  ],
  'non_existent': [],
  'symlink_full': [
    os.path.join('files1', 'subdir', '42.txt'),
    os.path.join('files1', 'test_file1.txt'),
    os.path.join('files1', 'test_file2.txt'),
    # files2 is a symlink to files1.
    'files2',
    'symlink_full.py',
  ],
  'symlink_partial': [
    os.path.join('files1', 'test_file2.txt'),
    # files2 is a symlink to files1.
    'files2',
    'symlink_partial.py',
  ],
  'touch_only': [
    'touch_only.py',
    os.path.join('files1', 'test_file1.txt'),
  ],
  'touch_root': [
    os.path.join('tests', 'isolate', 'touch_root.py'),
    'isolate.py',
  ],
  'with_flag': [
    'with_flag.py',
    os.path.join('files1', 'subdir', '42.txt'),
    os.path.join('files1', 'test_file1.txt'),
    os.path.join('files1', 'test_file2.txt'),
  ],
}


class CalledProcessError(subprocess.CalledProcessError):
  """Makes 2.6 version act like 2.7"""
  def __init__(self, returncode, cmd, output, stderr, cwd):
    super(CalledProcessError, self).__init__(returncode, cmd)
    self.output = output
    self.stderr = stderr
    self.cwd = cwd

  def __str__(self):
    return super(CalledProcessError, self).__str__() + (
        '\n'
        'cwd=%s\n%s\n%s\n%s') % (
            self.cwd,
            self.output,
            self.stderr,
            ' '.join(self.cmd))


def list_files_tree(directory):
  """Returns the list of all the files in a tree."""
  actual = []
  for root, dirnames, filenames in os.walk(directory):
    actual.extend(os.path.join(root, f)[len(directory)+1:] for f in filenames)
    for dirname in dirnames:
      full = os.path.join(root, dirname)
      # Manually include symlinks.
      if os.path.islink(full):
        actual.append(full[len(directory)+1:])
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
    logging.debug(self.tempdir)
    shutil.rmtree(self.tempdir)

  @staticmethod
  def _isolate_dict_to_string(values):
    buf = cStringIO.StringIO()
    isolate.pretty_print(values, buf)
    return buf.getvalue()

  @classmethod
  def _wrap_in_condition(cls, variables):
    """Wraps a variables dict inside the current OS condition.

    Returns the equivalent string.
    """
    return cls._isolate_dict_to_string(
        {
          'conditions': [
            ['OS=="%s"' % isolate.get_flavor(), {
              'variables': variables
            }],
          ],
        })


class IsolateModeBase(IsolateBase):
  def _expect_no_tree(self):
    self.assertFalse(os.path.exists(self.outdir))

  def _result_tree(self):
    return list_files_tree(self.outdir)

  def _expected_tree(self):
    """Verifies the files written in the temporary directory."""
    self.assertEquals(sorted(DEPENDENCIES[self.case()]), self._result_tree())

  @staticmethod
  def _fix_file_mode(filename, read_only):
    """4 modes are supported, 0750 (rwx), 0640 (rw), 0550 (rx), 0440 (r)."""
    min_mode = 0440
    if not read_only:
      min_mode |= 0200
    return (min_mode | 0110) if filename.endswith('.py') else min_mode

  def _gen_files(self, read_only, empty_file):
    """Returns a dict of files like calling isolate.process_input() on each
    file.
    """
    root_dir = ROOT_DIR
    if RELATIVE_CWD[self.case()] == '.':
      root_dir = os.path.join(root_dir, 'tests', 'isolate')

    files = dict((unicode(f), {}) for f in DEPENDENCIES[self.case()])

    for relfile, v in files.iteritems():
      filepath = os.path.join(root_dir, relfile)
      if self.LEVEL >= isolate.STATS_ONLY:
        filestats = os.lstat(filepath)
        is_link = stat.S_ISLNK(filestats.st_mode)
        if not is_link:
          v[u'size'] = filestats.st_size
          if isolate.get_flavor() != 'win':
            v[u'mode'] = self._fix_file_mode(relfile, read_only)
        else:
          v[u'mode'] = 488
        # Used the skip recalculating the hash. Use the most recent update
        # time.
        v[u'timestamp'] = int(round(filestats.st_mtime))
        if is_link:
          v['link'] = os.readlink(filepath)

      if self.LEVEL >= isolate.WITH_HASH:
        if not is_link:
          # Upgrade the value to unicode so diffing the structure in case of
          # test failure is easier, since the basestring type must match,
          # str!=unicode.
          v[u'sha-1'] = unicode(calc_sha1(filepath))

    if empty_file:
      item = files[empty_file]
      item['sha-1'] = unicode(SHA_1_NULL)
      if sys.platform != 'win32':
        item['mode'] = 288
      item['size'] = 0
      item['touched_only'] = True
      item.pop('timestamp', None)
    return files

  def _expected_result(self, args, read_only, empty_file):
    """Verifies self.result contains the expected data."""
    expected = {
      u'files': self._gen_files(read_only, empty_file),
      u'os': isolate.get_flavor(),
      u'relative_cwd': unicode(RELATIVE_CWD[self.case()]),
    }
    if read_only is not None:
      expected[u'read_only'] = read_only
    if args:
      expected[u'command'] = [u'python'] + [unicode(x) for x in args]
    else:
      expected[u'command'] = []
    self.assertEquals(expected, json.load(open(self.result, 'r')))

  def _expected_saved_state(self, extra_vars):
    flavor = isolate.get_flavor()
    expected = {
      u'isolate_file': unicode(self.filename()),
      u'variables': {
        u'EXECUTABLE_SUFFIX': '.exe' if flavor == 'win' else '',
        u'OS': unicode(flavor),
      },
    }
    expected['variables'].update(extra_vars or {})
    self.assertEquals(expected, json.load(open(self.saved_state(), 'r')))

  def _expect_results(self, args, read_only, extra_vars, empty_file):
    self._expected_result(args, read_only, empty_file)
    self._expected_saved_state(extra_vars)
    # Also verifies run_test_from_archive.py will be able to read it.
    run_test_from_archive.load_manifest(open(self.result, 'r').read())

  def _expect_no_result(self):
    self.assertFalse(os.path.exists(self.result))

  def _execute(self, mode, case, args, need_output):
    """Executes isolate.py."""
    self.assertEquals(
        case,
        self.case() + '.isolate',
        'Rename the test case to test_%s()' % case)
    cmd = [
      sys.executable, os.path.join(ROOT_DIR, 'isolate.py'),
      mode,
      '--result', self.result,
      '--outdir', self.outdir,
      '--isolate', self.filename(),
    ]
    cmd.extend(args)

    env = os.environ.copy()
    if 'ISOLATE_DEBUG' in env:
      del env['ISOLATE_DEBUG']

    if need_output or not VERBOSE:
      stdout = subprocess.PIPE
      stderr = subprocess.PIPE
    else:
      cmd.extend(['-v'] * 3)
      stdout = None
      stderr = None

    logging.debug(cmd)
    cwd = ROOT_DIR
    p = subprocess.Popen(
        cmd,
        stdout=stdout,
        stderr=stderr,
        cwd=cwd,
        env=env,
        universal_newlines=True)
    out, err = p.communicate()
    if p.returncode:
      raise CalledProcessError(p.returncode, cmd, out, err, cwd)

    # Do not check on Windows since a lot of spew is generated there.
    if sys.platform != 'win32':
      self.assertTrue(err in (None, ''), err)
    return out

  def case(self):
    """Returns the filename corresponding to this test case."""
    test_id = self.id().split('.')
    return re.match('^test_([a-z_]+)$', test_id[2]).group(1)

  def filename(self):
    """Returns the filename corresponding to this test case."""
    filename = os.path.join(
        ROOT_DIR, 'tests', 'isolate', self.case() + '.isolate')
    self.assertTrue(os.path.isfile(filename), filename)
    return filename

  def saved_state(self):
    return isolate.result_to_state(self.result)


class Isolate(unittest.TestCase):
  # Does not inherit from the other *Base classes.
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
    self.assertEquals(sorted(EXPECTED_MODES), sorted(modes))

  def test_modes(self):
    # This is a bit redundant but make sure all combinations are tested.
    files = sorted(
      i[:-len('.isolate')]
      for i in os.listdir(os.path.join(ROOT_DIR, 'tests', 'isolate'))
      if i.endswith('.isolate')
    )
    self.assertEquals(sorted(RELATIVE_CWD), files)
    self.assertEquals(sorted(DEPENDENCIES), files)

    if sys.platform == 'win32':
      # Symlink stuff is unsupported there, remove them from the list.
      files = [f for f in files if not f.startswith('symlink_')]

    # modes read and trace are tested together.
    modes_to_check = list(EXPECTED_MODES)
    modes_to_check.remove('help')
    modes_to_check.remove('merge')
    modes_to_check.remove('noop')
    modes_to_check.remove('read')
    modes_to_check.remove('trace')
    modes_to_check.append('trace_read_merge')
    for mode in modes_to_check:
      expected_cases = set('test_%s' % f for f in files)
      fixture_name = 'Isolate_%s' % mode
      fixture = getattr(sys.modules[__name__], fixture_name)
      actual_cases = set(i for i in dir(fixture) if i.startswith('test_'))
      missing = expected_cases - actual_cases
      self.assertFalse(missing, '%s.%s' % (fixture_name, missing))


class Isolate_check(IsolateModeBase):
  LEVEL = isolate.NO_INFO

  def test_fail(self):
    self._execute('check', 'fail.isolate', [], False)
    self._expect_no_tree()
    self._expect_results(['fail.py'], None, None, None)

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
    self._expect_results([], None, None, None)

  def test_touch_only(self):
    self._execute('check', 'touch_only.isolate', ['-V', 'FLAG', 'gyp'], False)
    self._expect_no_tree()
    empty = os.path.join('files1', 'test_file1.txt')
    self._expected_result(['touch_only.py', 'gyp'], None, empty)

  def test_touch_root(self):
    self._execute('check', 'touch_root.isolate', [], False)
    self._expect_no_tree()
    self._expect_results(['touch_root.py'], None, None, None)

  def test_with_flag(self):
    self._execute('check', 'with_flag.isolate', ['-V', 'FLAG', 'gyp'], False)
    self._expect_no_tree()
    self._expect_results(
        ['with_flag.py', 'gyp'], None, {u'FLAG': u'gyp'}, None)

  if sys.platform != 'win32':
    def test_symlink_full(self):
      self._execute('check', 'symlink_full.isolate', [], False)
      self._expect_no_tree()
      self._expect_results(['symlink_full.py'], None, None, None)

    def test_symlink_partial(self):
      self._execute('check', 'symlink_partial.isolate', [], False)
      self._expect_no_tree()
      self._expect_results(['symlink_partial.py'], None, None, None)


class Isolate_hashtable(IsolateModeBase):
  LEVEL = isolate.WITH_HASH

  def _gen_expected_tree(self, empty_file):
    expected = [
      v['sha-1'] for v in self._gen_files(False, empty_file).itervalues()
    ]
    expected.append(calc_sha1(self.result))
    return expected

  def _expected_hash_tree(self, empty_file):
    """Verifies the files written in the temporary directory."""
    self.assertEquals(
        sorted(self._gen_expected_tree(empty_file)), self._result_tree())

  def test_fail(self):
    self._execute('hashtable', 'fail.isolate', [], False)
    self._expected_hash_tree(None)
    self._expect_results(['fail.py'], None, None, None)

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
    self._expected_hash_tree(None)
    self._expect_results([], None, None, None)

  def test_touch_only(self):
    self._execute(
        'hashtable', 'touch_only.isolate', ['-V', 'FLAG', 'gyp'], False)
    empty = os.path.join('files1', 'test_file1.txt')
    self._expected_hash_tree(empty)
    self._expected_result(['touch_only.py', 'gyp'], None, empty)

  def test_touch_root(self):
    self._execute('hashtable', 'touch_root.isolate', [], False)
    self._expected_hash_tree(None)
    self._expect_results(['touch_root.py'], None, None, None)

  def test_with_flag(self):
    self._execute(
        'hashtable', 'with_flag.isolate', ['-V', 'FLAG', 'gyp'], False)
    self._expected_hash_tree(None)
    self._expect_results(
        ['with_flag.py', 'gyp'], None, {u'FLAG': u'gyp'}, None)

  if sys.platform != 'win32':
    def test_symlink_full(self):
      self._execute('hashtable', 'symlink_full.isolate', [], False)
      # Construct our own tree.
      expected = [
        str(v['sha-1'])
        for v in self._gen_files(False, None).itervalues() if 'sha-1' in v
      ]
      expected.append(calc_sha1(self.result))
      self.assertEquals(sorted(expected), self._result_tree())
      self._expect_results(['symlink_full.py'], None, None, None)

    def test_symlink_partial(self):
      self._execute('hashtable', 'symlink_partial.isolate', [], False)
      # Construct our own tree.
      expected = [
        str(v['sha-1'])
        for v in self._gen_files(False, None).itervalues() if 'sha-1' in v
      ]
      expected.append(calc_sha1(self.result))
      self.assertEquals(sorted(expected), self._result_tree())
      self._expect_results(['symlink_partial.py'], None, None, None)



class Isolate_remap(IsolateModeBase):
  LEVEL = isolate.STATS_ONLY

  def test_fail(self):
    self._execute('remap', 'fail.isolate', [], False)
    self._expected_tree()
    self._expect_results(['fail.py'], None, None, None)

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
    self._expect_results([], None, None, None)

  def test_touch_only(self):
    self._execute('remap', 'touch_only.isolate', ['-V', 'FLAG', 'gyp'], False)
    self._expected_tree()
    empty = os.path.join('files1', 'test_file1.txt')
    self._expect_results(
        ['touch_only.py', 'gyp'], None, {u'FLAG': u'gyp'}, empty)

  def test_touch_root(self):
    self._execute('remap', 'touch_root.isolate', [], False)
    self._expected_tree()
    self._expect_results(['touch_root.py'], None, None, None)

  def test_with_flag(self):
    self._execute('remap', 'with_flag.isolate', ['-V', 'FLAG', 'gyp'], False)
    self._expected_tree()
    self._expect_results(
        ['with_flag.py', 'gyp'], None, {u'FLAG': u'gyp'}, None)

  if sys.platform != 'win32':
    def test_symlink_full(self):
      self._execute('remap', 'symlink_full.isolate', [], False)
      self._expected_tree()
      self._expect_results(['symlink_full.py'], None, None, None)

    def test_symlink_partial(self):
      self._execute('remap', 'symlink_partial.isolate', [], False)
      self._expected_tree()
      self._expect_results(['symlink_partial.py'], None, None, None)


class Isolate_run(IsolateModeBase):
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
    self._expect_results(['fail.py'], None, None, None)

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
    self._expect_no_result()

  def test_touch_only(self):
    self._execute('run', 'touch_only.isolate', ['-V', 'FLAG', 'run'], False)
    self._expect_empty_tree()
    empty = os.path.join('files1', 'test_file1.txt')
    self._expect_results(
        ['touch_only.py', 'run'], None, {u'FLAG': u'run'}, empty)

  def test_touch_root(self):
    self._execute('run', 'touch_root.isolate', [], False)
    self._expect_empty_tree()
    self._expect_results(['touch_root.py'], None, None, None)

  def test_with_flag(self):
    self._execute('run', 'with_flag.isolate', ['-V', 'FLAG', 'run'], False)
    # Not sure about the empty tree, should be deleted.
    self._expect_empty_tree()
    self._expect_results(
        ['with_flag.py', 'run'], None, {u'FLAG': u'run'}, None)

  if sys.platform != 'win32':
    def test_symlink_full(self):
      self._execute('run', 'symlink_full.isolate', [], False)
      self._expect_empty_tree()
      self._expect_results(['symlink_full.py'], None, None, None)

    def test_symlink_partial(self):
      self._execute('run', 'symlink_partial.isolate', [], False)
      self._expect_empty_tree()
      self._expect_results(['symlink_partial.py'], None, None, None)


class Isolate_trace_read_merge(IsolateModeBase):
  # Tests both trace, read and merge.
  # Warning: merge updates .isolate files. But they are currently in their
  # canonical format so they shouldn't be changed.
  LEVEL = isolate.STATS_ONLY

  def _check_merge(self, filename):
    filepath = isolate.trace_inputs.get_native_path_case(
        os.path.join(ROOT_DIR, 'tests', 'isolate', filename))
    expected = 'Updating %s\n' % filepath
    with open(filepath, 'rb') as f:
      old_content = f.read()
    out = self._execute('merge', filename, [], True) or ''
    self.assertEquals(expected, out)
    with open(filepath, 'rb') as f:
      new_content = f.read()
    self.assertEquals(old_content, new_content)

  def test_fail(self):
    # Even if the process returns non-zero, the trace will still be good.
    try:
      self._execute('trace', 'fail.isolate', ['-v'], True)
      self.fail()
    except subprocess.CalledProcessError, e:
      self.assertEquals('', e.output)
    self._expect_no_tree()
    self._expect_results(['fail.py'], None, None, None)
    expected = self._wrap_in_condition(
        {
          isolate.KEY_TRACKED: [
            'fail.py',
          ],
        })
    out = self._execute('read', 'fail.isolate', [], True) or ''
    self.assertEquals(expected.splitlines(), out.splitlines())
    self._check_merge('fail.isolate')

  def test_missing_trailing_slash(self):
    try:
      self._execute('trace', 'missing_trailing_slash.isolate', [], True)
      self.fail()
    except subprocess.CalledProcessError, e:
      self.assertEquals('', e.output)
      out = e.stderr
    self._expect_no_tree()
    self._expect_no_result()
    expected = (
      '\n'
      'Error: Input directory %s must have a trailing slash\n' %
          os.path.join(ROOT_DIR, 'tests', 'isolate', 'files1')
    )
    self.assertEquals(expected, out)

  def test_non_existent(self):
    try:
      self._execute('trace', 'non_existent.isolate', [], True)
      self.fail()
    except subprocess.CalledProcessError, e:
      self.assertEquals('', e.output)
      out = e.stderr
    self._expect_no_tree()
    self._expect_no_result()
    expected = (
      '\n'
      'Error: Input file %s doesn\'t exist\n' %
          os.path.join(ROOT_DIR, 'tests', 'isolate', 'A_file_that_do_not_exist')
    )
    self.assertEquals(expected, out)

  def test_no_run(self):
    try:
      self._execute('trace', 'no_run.isolate', [], True)
      self.fail()
    except subprocess.CalledProcessError, e:
      out = e.output
      err = e.stderr
    self._expect_no_tree()
    self._expect_no_result()
    expected = '\nError: No command to run\n'
    self.assertEquals('', out)
    self.assertEquals(expected, err)

  def test_touch_only(self):
    out = self._execute(
        'trace', 'touch_only.isolate', ['-V', 'FLAG', 'trace'], True)
    self.assertEquals('', out)
    self._expect_no_tree()
    empty = os.path.join('files1', 'test_file1.txt')
    self._expect_results(
        ['touch_only.py', 'trace'], None, {u'FLAG': u'trace'}, empty)
    expected = {
      isolate.KEY_TRACKED: [
        'touch_only.py',
      ],
      isolate.KEY_TOUCHED: [
        # Note that .isolate format mandates / and not os.path.sep.
        'files1/test_file1.txt',
      ],
    }
    if sys.platform != 'linux2':
      # TODO(maruel): Implement touch-only tracing on non-linux.
      del expected[isolate.KEY_TOUCHED]

    out = self._execute('read', 'touch_only.isolate', [], True)
    self.assertEquals(self._wrap_in_condition(expected), out)
    self._check_merge('touch_only.isolate')

  def test_touch_root(self):
    out = self._execute('trace', 'touch_root.isolate', [], True)
    self.assertEquals('', out)
    self._expect_no_tree()
    self._expect_results(['touch_root.py'], None, None, None)
    expected = self._wrap_in_condition(
        {
          isolate.KEY_TRACKED: [
            '../../isolate.py',
            'touch_root.py',
          ],
        })
    out = self._execute('read', 'touch_root.isolate', [], True)
    self.assertEquals(expected, out)
    self._check_merge('touch_root.isolate')

  def test_with_flag(self):
    out = self._execute(
        'trace', 'with_flag.isolate', ['-V', 'FLAG', 'trace'], True)
    self.assertEquals('', out)
    self._expect_no_tree()
    self._expect_results(
        ['with_flag.py', 'trace'], None, {u'FLAG': u'trace'}, None)
    expected = {
      isolate.KEY_TRACKED: [
        'with_flag.py',
      ],
      isolate.KEY_UNTRACKED: [
        # Note that .isolate format mandates / and not os.path.sep.
        'files1/',
      ],
    }
    out = self._execute('read', 'with_flag.isolate', [], True)
    self.assertEquals(self._wrap_in_condition(expected), out)
    self._check_merge('with_flag.isolate')

  if sys.platform != 'win32':
    def test_symlink_full(self):
      out = self._execute(
          'trace', 'symlink_full.isolate', [], True)
      self.assertEquals('', out)
      self._expect_no_tree()
      self._expect_results(['symlink_full.py'], None, None, None)
      expected = {
        isolate.KEY_TRACKED: [
          'symlink_full.py',
        ],
        isolate.KEY_UNTRACKED: [
          # Note that .isolate format mandates / and not os.path.sep.
          'files2/',
        ],
      }
      out = self._execute('read', 'symlink_full.isolate', [], True)
      self.assertEquals(self._wrap_in_condition(expected), out)
      self._check_merge('symlink_full.isolate')

    def test_symlink_partial(self):
      out = self._execute(
          'trace', 'symlink_partial.isolate', [], True)
      self.assertEquals('', out)
      self._expect_no_tree()
      self._expect_results(['symlink_partial.py'], None, None, None)
      expected = {
        isolate.KEY_TRACKED: [
          'symlink_partial.py',
        ],
        isolate.KEY_UNTRACKED: [
          'files2/test_file2.txt',
        ],
      }
      out = self._execute('read', 'symlink_partial.isolate', [], True)
      self.assertEquals(self._wrap_in_condition(expected), out)
      self._check_merge('symlink_partial.isolate')


class IsolateNoOutdir(IsolateBase):
  # Test without the --outdir flag.
  # So all the files are first copied in the tempdir and the test is run from
  # there.
  def setUp(self):
    super(IsolateNoOutdir, self).setUp()
    self.root = os.path.join(self.tempdir, 'root')
    os.makedirs(os.path.join(self.root, 'tests', 'isolate'))
    for i in ('touch_root.isolate', 'touch_root.py'):
      shutil.copy(
          os.path.join(ROOT_DIR, 'tests', 'isolate', i),
          os.path.join(self.root, 'tests', 'isolate', i))
      shutil.copy(
          os.path.join(ROOT_DIR, 'isolate.py'),
          os.path.join(self.root, 'isolate.py'))

  def _execute(self, mode, args, need_output):
    """Executes isolate.py."""
    cmd = [
      sys.executable, os.path.join(ROOT_DIR, 'isolate.py'),
      mode,
      '--result', self.result,
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

    logging.debug(cmd)
    cwd = self.tempdir
    p = subprocess.Popen(
        cmd,
        stdout=stdout,
        stderr=stderr,
        cwd=cwd,
        env=env,
        universal_newlines=True)
    out, err = p.communicate()
    if p.returncode:
      raise CalledProcessError(p.returncode, cmd, out, err, cwd)
    return out

  def mode(self):
    """Returns the execution mode corresponding to this test case."""
    test_id = self.id().split('.')
    self.assertEquals(3, len(test_id))
    self.assertEquals('__main__', test_id[0])
    return re.match('^test_([a-z]+)$', test_id[2]).group(1)

  def filename(self):
    """Returns the filename corresponding to this test case."""
    filename = os.path.join(self.root, 'tests', 'isolate', 'touch_root.isolate')
    self.assertTrue(os.path.isfile(filename), filename)
    return filename

  def test_check(self):
    self._execute('check', ['--isolate', self.filename()], False)
    files = sorted([
      'isolate_smoke_test.results',
      'isolate_smoke_test.state',
      os.path.join('root', 'tests', 'isolate', 'touch_root.isolate'),
      os.path.join('root', 'tests', 'isolate', 'touch_root.py'),
      os.path.join('root', 'isolate.py'),
    ])
    self.assertEquals(files, list_files_tree(self.tempdir))

  def test_hashtable(self):
    self._execute('hashtable', ['--isolate', self.filename()], False)
    files = sorted([
      os.path.join(
          'hashtable', calc_sha1(os.path.join(ROOT_DIR, 'isolate.py'))),
      os.path.join(
          'hashtable',
          calc_sha1(
              os.path.join(ROOT_DIR, 'tests', 'isolate', 'touch_root.py'))),
      os.path.join('hashtable', calc_sha1(os.path.join(self.result))),
      'isolate_smoke_test.results',
      'isolate_smoke_test.state',
      os.path.join('root', 'tests', 'isolate', 'touch_root.isolate'),
      os.path.join('root', 'tests', 'isolate', 'touch_root.py'),
      os.path.join('root', 'isolate.py'),
    ])
    self.assertEquals(files, list_files_tree(self.tempdir))

  def test_remap(self):
    self._execute('remap', ['--isolate', self.filename()], False)
    files = sorted([
      'isolate_smoke_test.results',
      'isolate_smoke_test.state',
      os.path.join('root', 'tests', 'isolate', 'touch_root.isolate'),
      os.path.join('root', 'tests', 'isolate', 'touch_root.py'),
      os.path.join('root', 'isolate.py'),
    ])
    self.assertEquals(files, list_files_tree(self.tempdir))

  def test_run(self):
    self._execute('run', ['--isolate', self.filename()], False)
    files = sorted([
      'isolate_smoke_test.results',
      'isolate_smoke_test.state',
      os.path.join('root', 'tests', 'isolate', 'touch_root.isolate'),
      os.path.join('root', 'tests', 'isolate', 'touch_root.py'),
      os.path.join('root', 'isolate.py'),
    ])
    self.assertEquals(files, list_files_tree(self.tempdir))

  def test_trace_read_merge(self):
    self._execute('trace', ['--isolate', self.filename()], False)
    # Read the trace before cleaning up. No need to specify self.filename()
    # because add the needed information is in the .state file.
    output = self._execute('read', [], True)
    expected = {
      isolate.KEY_TRACKED: [
        '../../isolate.py',
        'touch_root.py',
      ],
    }
    self.assertEquals(self._wrap_in_condition(expected), output)

    output = self._execute('merge', [], True)
    expected = 'Updating %s\n' % isolate.trace_inputs.get_native_path_case(
        os.path.join(self.root, 'tests', 'isolate', 'touch_root.isolate'))
    self.assertEquals(expected, output)
    # In theory the file is going to be updated but in practice its content
    # won't change.

    # Clean the directory from the logs, which are OS-specific.
    isolate.trace_inputs.get_api().clean_trace(
        os.path.join(self.tempdir, 'isolate_smoke_test.results.log'))
    files = sorted([
      'isolate_smoke_test.results',
      'isolate_smoke_test.state',
      os.path.join('root', 'tests', 'isolate', 'touch_root.isolate'),
      os.path.join('root', 'tests', 'isolate', 'touch_root.py'),
      os.path.join('root', 'isolate.py'),
    ])
    self.assertEquals(files, list_files_tree(self.tempdir))


if __name__ == '__main__':
  VERBOSE = '-v' in sys.argv
  logging.basicConfig(level=logging.DEBUG if VERBOSE else logging.ERROR)
  unittest.main()
