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


VERBOSE = False
SHA_1_NULL = hashlib.sha1().hexdigest()


# Keep the list hard coded.
EXPECTED_MODES = (
  'check',
  'hashtable',
  'help',
  'merge',
  'read',
  'remap',
  'rewrite',
  'run',
  'trace',
)

# These are per test case, not per mode.
RELATIVE_CWD = {
  'all_items_invalid': '.',
  'fail': '.',
  'missing_trailing_slash': '.',
  'no_run': '.',
  'non_existent': '.',
  'split': '.',
  'symlink_full': '.',
  'symlink_partial': '.',
  'symlink_outside_build_root': '.',
  'touch_only': '.',
  'touch_root': os.path.join('tests', 'isolate'),
  'with_flag': '.',
}

DEPENDENCIES = {
  'all_items_invalid' : ['empty.py'],
  'fail': ['fail.py'],
  'missing_trailing_slash': [],
  'no_run': [
    'no_run.isolate',
    os.path.join('files1', 'subdir', '42.txt'),
    os.path.join('files1', 'test_file1.txt'),
    os.path.join('files1', 'test_file2.txt'),
  ],
  'non_existent': [],
  'split': [
    os.path.join('files1', 'subdir', '42.txt'),
    os.path.join('test', 'data', 'foo.txt'),
    'split.py',
  ],
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
  'symlink_outside_build_root': [
    os.path.join('link_outside_build_root', 'test_file3.txt'),
    'symlink_outside_build_root.py',
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
  def setUp(self):
    # The tests assume the current directory is the file's directory.
    os.chdir(ROOT_DIR)
    self.tempdir = tempfile.mkdtemp()
    self.isolated = os.path.join(self.tempdir, 'isolate_smoke_test.isolated')
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
    flavor = isolate.get_flavor()
    chromeos_value = int(flavor == 'linux')
    return cls._isolate_dict_to_string(
        {
          'conditions': [
            ['OS=="%s" and chromeos==%d' % (flavor, chromeos_value), {
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

  def _gen_files(self, read_only, empty_file, with_time):
    """Returns a dict of files like calling isolate.process_input() on each
    file.

    Arguments:
    - read_only: Mark all the 'm' modes without the writeable bit.
    - empty_file: Add a specific empty file (size 0).
    - with_time: Include 't' timestamps. For saved state .state files.
    """
    root_dir = ROOT_DIR
    if RELATIVE_CWD[self.case()] == '.':
      root_dir = os.path.join(root_dir, 'tests', 'isolate')

    files = dict((unicode(f), {}) for f in DEPENDENCIES[self.case()])

    for relfile, v in files.iteritems():
      filepath = os.path.join(root_dir, relfile)
      filestats = os.lstat(filepath)
      is_link = stat.S_ISLNK(filestats.st_mode)
      if not is_link:
        v[u's'] = filestats.st_size
        if isolate.get_flavor() != 'win':
          v[u'm'] = self._fix_file_mode(relfile, read_only)
      else:
        v[u'm'] = 488
      if with_time:
        # Used to skip recalculating the hash. Use the most recent update
        # time.
        v[u't'] = int(round(filestats.st_mtime))
      if is_link:
        v['l'] = os.readlink(filepath)  # pylint: disable=E1101
      else:
        # Upgrade the value to unicode so diffing the structure in case of
        # test failure is easier, since the basestring type must match,
        # str!=unicode.
        v[u'h'] = unicode(calc_sha1(filepath))

    if empty_file:
      item = files[empty_file]
      item['h'] = unicode(SHA_1_NULL)
      if sys.platform != 'win32':
        item['m'] = 288
      item['s'] = 0
      if with_time:
        item['T'] = True
        item.pop('t', None)
    return files

  def _expected_result(self, args, read_only, empty_file):
    """Verifies self.isolated contains the expected data."""
    expected = {
      u'files': self._gen_files(read_only, empty_file, False),
      u'os': unicode(isolate.get_flavor()),
      u'relative_cwd': unicode(RELATIVE_CWD[self.case()]),
    }
    if read_only is not None:
      expected[u'read_only'] = read_only
    if args:
      expected[u'command'] = [u'python'] + [unicode(x) for x in args]
    self.assertEquals(expected, json.load(open(self.isolated, 'r')))

  def _expected_saved_state(self, args, read_only, empty_file, extra_vars):
    flavor = isolate.get_flavor()
    chromeos_value = int(flavor == 'linux')
    expected = {
      u'command': [],
      u'files': self._gen_files(read_only, empty_file, True),
      u'isolate_file': isolate.trace_inputs.get_native_path_case(
          unicode(self.filename())),
      u'isolated_files': [self.isolated],
      u'relative_cwd': unicode(RELATIVE_CWD[self.case()]),
      u'variables': {
        u'EXECUTABLE_SUFFIX': u'.exe' if flavor == 'win' else u'',
        u'OS': unicode(flavor),
        u'chromeos': chromeos_value,
      },
    }
    if args:
      expected[u'command'] = [u'python'] + [unicode(x) for x in args]
    expected['variables'].update(extra_vars or {})
    self.assertEquals(expected, json.load(open(self.saved_state(), 'r')))

  def _expect_results(self, args, read_only, extra_vars, empty_file):
    self._expected_result(args, read_only, empty_file)
    self._expected_saved_state(args, read_only, empty_file, extra_vars)
    # Also verifies run_isolated.py will be able to read it.
    isolate.run_isolated.load_isolated(open(self.isolated, 'r').read())

  def _expect_no_result(self):
    self.assertFalse(os.path.exists(self.isolated))

  def _execute(self, mode, case, args, need_output, cwd=ROOT_DIR):
    """Executes isolate.py."""
    self.assertEquals(
        case,
        self.case() + '.isolate',
        'Rename the test case to test_%s()' % case)
    chromeos_value = int(isolate.get_flavor() == 'linux')
    cmd = [
      sys.executable, os.path.join(ROOT_DIR, 'isolate.py'),
      mode,
      '--isolated', self.isolated,
      '--outdir', self.outdir,
      '--isolate', self.filename(),
      '-V', 'chromeos', str(chromeos_value),
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
    return isolate.isolatedfile_to_state(self.isolated)

  def _test_missing_trailing_slash(self, mode):
    try:
      self._execute(mode, 'missing_trailing_slash.isolate', [], True)
      self.fail()
    except subprocess.CalledProcessError as e:
      self.assertEquals('', e.output)
      out = e.stderr
    self._expect_no_tree()
    self._expect_no_result()
    root = isolate.trace_inputs.get_native_path_case(unicode(ROOT_DIR))
    expected = (
      '\n'
      'Error: Input directory %s must have a trailing slash\n' %
          os.path.join(root, 'tests', 'isolate', 'files1')
    )
    self.assertEquals(expected, out)

  def _test_non_existent(self, mode):
    try:
      self._execute(mode, 'non_existent.isolate', [], True)
      self.fail()
    except subprocess.CalledProcessError as e:
      self.assertEquals('', e.output)
      out = e.stderr
    self._expect_no_tree()
    self._expect_no_result()
    root = isolate.trace_inputs.get_native_path_case(unicode(ROOT_DIR))
    expected = (
      '\n'
      'Error: Input file %s doesn\'t exist\n' %
          os.path.join(root, 'tests', 'isolate', 'A_file_that_do_not_exist')
    )
    self.assertEquals(expected, out)

  def _test_all_items_invalid(self, mode):
    out = self._execute(mode, 'all_items_invalid.isolate',
                        ['--ignore_broken_item'], True)
    self._expect_results(['empty.py'], None, None, None)

    return out or ''


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

    files.remove('split')
    # TODO(csharp): touch_only is disabled until crbug.com/150823 is fixed.
    files.remove('touch_only')

    # modes read and trace are tested together.
    modes_to_check = list(EXPECTED_MODES)
    # Tested with test_help_modes.
    modes_to_check.remove('help')
    # Tested with trace_read_merge.
    modes_to_check.remove('merge')
    # Tested with trace_read_merge.
    modes_to_check.remove('read')
    # Tested in isolate_test.py.
    modes_to_check.remove('rewrite')
    # Tested with trace_read_merge.
    modes_to_check.remove('trace')
    modes_to_check.append('trace_read_merge')
    for mode in modes_to_check:
      expected_cases = set('test_%s' % f for f in files)
      fixture_name = 'Isolate_%s' % mode
      fixture = getattr(sys.modules[__name__], fixture_name)
      actual_cases = set(i for i in dir(fixture) if i.startswith('test_'))
      actual_cases.discard('split')
      missing = expected_cases - actual_cases
      self.assertFalse(missing, '%s.%s' % (fixture_name, missing))


class Isolate_check(IsolateModeBase):
  def test_fail(self):
    self._execute('check', 'fail.isolate', [], False)
    self._expect_no_tree()
    self._expect_results(['fail.py'], None, None, None)

  def test_missing_trailing_slash(self):
    self._test_missing_trailing_slash('check')

  def test_non_existent(self):
    self._test_non_existent('check')

  def test_all_items_invalid(self):
    out = self._test_all_items_invalid('check')
    self.assertEquals('', out)
    self._expect_no_tree()

  def test_no_run(self):
    self._execute('check', 'no_run.isolate', [], False)
    self._expect_no_tree()
    self._expect_results([], None, None, None)

  # TODO(csharp): Disabled until crbug.com/150823 is fixed.
  def do_not_test_touch_only(self):
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

    def test_symlink_outside_build_root(self):
      self._execute('check', 'symlink_outside_build_root.isolate', [], False)
      self._expect_no_tree()
      self._expect_results(['symlink_outside_build_root.py'], None, None, None)


class Isolate_hashtable(IsolateModeBase):
  def _gen_expected_tree(self, empty_file):
    expected = [
      unicode(v['h'])
      for v in self._gen_files(False, empty_file, False).itervalues()
    ]
    expected.append(unicode(calc_sha1(self.isolated)))
    return expected

  def _expected_hash_tree(self, empty_file):
    """Verifies the files written in the temporary directory."""
    self.assertEquals(
        sorted(self._gen_expected_tree(empty_file)),
        map(unicode, self._result_tree()))

  def test_fail(self):
    self._execute('hashtable', 'fail.isolate', [], False)
    self._expected_hash_tree(None)
    self._expect_results(['fail.py'], None, None, None)

  def test_missing_trailing_slash(self):
    self._test_missing_trailing_slash('hashtable')

  def test_non_existent(self):
    self._test_non_existent('hashtable')

  def test_all_items_invalid(self):
    out = self._test_all_items_invalid('hashtable')
    self.assertEquals('', out)
    self._expected_hash_tree(None)

  def test_no_run(self):
    self._execute('hashtable', 'no_run.isolate', [], False)
    self._expected_hash_tree(None)
    self._expect_results([], None, None, None)

  def test_split(self):
    self._execute(
        'hashtable',
        'split.isolate',
        ['-V', 'DEPTH', '.', '-V', 'PRODUCT_DIR', 'files1'],
        False,
        cwd=os.path.join(ROOT_DIR, 'tests', 'isolate'))
    # Reimplement _expected_hash_tree():
    tree = self._gen_expected_tree(None)
    isolated_base = self.isolated[:-len('.isolated')]
    isolated_hashes = [
      unicode(calc_sha1(isolated_base + '.0.isolated')),
      unicode(calc_sha1(isolated_base + '.1.isolated')),
    ]
    tree.extend(isolated_hashes)
    self.assertEquals(sorted(tree), map(unicode, self._result_tree()))

    # Reimplement _expected_result():
    files = self._gen_files(None, None, False)
    expected = {
      u'command': [u'python', u'split.py'],
      u'files': {u'split.py': files['split.py']},
      u'includes': isolated_hashes,
      u'os': unicode(isolate.get_flavor()),
      u'relative_cwd': unicode(RELATIVE_CWD[self.case()]),
    }
    self.assertEquals(expected, json.load(open(self.isolated, 'r')))

    key = os.path.join(u'test', 'data', 'foo.txt')
    expected = {
      u'files': {key: files[key]},
      u'os': unicode(isolate.get_flavor()),
    }
    self.assertEquals(
        expected, json.load(open(isolated_base + '.0.isolated', 'r')))

    key = os.path.join(u'files1', 'subdir', '42.txt')
    expected = {
      u'files': {key: files[key]},
      u'os': unicode(isolate.get_flavor()),
    }
    self.assertEquals(
        expected, json.load(open(isolated_base + '.1.isolated', 'r')))


  # TODO(csharp): Disabled until crbug.com/150823 is fixed.
  def do_not_test_touch_only(self):
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
        str(v['h'])
        for v in self._gen_files(False, None, False).itervalues() if 'h' in v
      ]
      expected.append(calc_sha1(self.isolated))
      self.assertEquals(sorted(expected), self._result_tree())
      self._expect_results(['symlink_full.py'], None, None, None)

    def test_symlink_partial(self):
      self._execute('hashtable', 'symlink_partial.isolate', [], False)
      # Construct our own tree.
      expected = [
        str(v['h'])
        for v in self._gen_files(False, None, False).itervalues() if 'h' in v
      ]
      expected.append(calc_sha1(self.isolated))
      self.assertEquals(sorted(expected), self._result_tree())
      self._expect_results(['symlink_partial.py'], None, None, None)

    def test_symlink_outside_build_root(self):
      self._execute(
          'hashtable', 'symlink_outside_build_root.isolate', [], False)
      # Construct our own tree.
      expected = [
        str(v['h'])
        for v in self._gen_files(False, None, False).itervalues() if 'h' in v
      ]
      expected.append(calc_sha1(self.isolated))
      self.assertEquals(sorted(expected), self._result_tree())
      self._expect_results(['symlink_outside_build_root.py'], None, None, None)


class Isolate_remap(IsolateModeBase):
  def test_fail(self):
    self._execute('remap', 'fail.isolate', [], False)
    self._expected_tree()
    self._expect_results(['fail.py'], None, None, None)

  def test_missing_trailing_slash(self):
    self._test_missing_trailing_slash('remap')

  def test_non_existent(self):
    self._test_non_existent('remap')

  def test_all_items_invalid(self):
    out = self._test_all_items_invalid('remap')
    self.assertTrue(out.startswith('Remapping'))
    self._expected_tree()

  def test_no_run(self):
    self._execute('remap', 'no_run.isolate', [], False)
    self._expected_tree()
    self._expect_results([], None, None, None)

  # TODO(csharp): Disabled until crbug.com/150823 is fixed.
  def do_not_test_touch_only(self):
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

    def test_symlink_outside_build_root(self):
      self._execute('remap', 'symlink_outside_build_root.isolate', [], False)
      self._expected_tree()
      self._expect_results(['symlink_outside_build_root.py'], None, None, None)


class Isolate_run(IsolateModeBase):
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
    self._test_missing_trailing_slash('run')

  def test_non_existent(self):
    self._test_non_existent('run')

  def test_all_items_invalid(self):
    out = self._test_all_items_invalid('run')
    self.assertEqual('', out)
    self._expect_no_tree()

  def test_no_run(self):
    try:
      self._execute('run', 'no_run.isolate', [], False)
      self.fail()
    except subprocess.CalledProcessError:
      pass
    self._expect_empty_tree()
    self._expect_no_result()

  # TODO(csharp): Disabled until crbug.com/150823 is fixed.
  def do_not_test_touch_only(self):
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

    def test_symlink_outside_build_root(self):
      self._execute('run', 'symlink_outside_build_root.isolate', [], False)
      self._expect_empty_tree()
      self._expect_results(['symlink_outside_build_root.py'], None, None, None)


class Isolate_trace_read_merge(IsolateModeBase):
  # Tests both trace, read and merge.
  # Warning: merge updates .isolate files. But they are currently in their
  # canonical format so they shouldn't be changed.
  def _check_merge(self, filename):
    filepath = isolate.trace_inputs.get_native_path_case(
        os.path.join(unicode(ROOT_DIR), 'tests', 'isolate', filename))
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
    self._test_missing_trailing_slash('trace')

  def test_non_existent(self):
    self._test_non_existent('trace')

  def test_all_items_invalid(self):
    out = self._test_all_items_invalid('trace')
    self.assertEqual('', out)
    self._expect_no_tree()

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

  # TODO(csharp): Disabled until crbug.com/150823 is fixed.
  def do_not_test_touch_only(self):
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

    def test_symlink_outside_build_root(self):
      out = self._execute(
          'trace', 'symlink_outside_build_root.isolate', [], True)
      self.assertEquals('', out)
      self._expect_no_tree()
      self._expect_results(['symlink_outside_build_root.py'], None, None, None)
      expected = {
        isolate.KEY_TRACKED: [
          'symlink_outside_build_root.py',
        ],
        isolate.KEY_UNTRACKED: [
          'link_outside_build_root/',
        ],
      }
      out = self._execute(
          'read', 'symlink_outside_build_root.isolate', [], True)
      self.assertEquals(self._wrap_in_condition(expected), out)
      self._check_merge('symlink_outside_build_root.isolate')


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
    chromeos_value = int(isolate.get_flavor() == 'linux')
    cmd = [
      sys.executable, os.path.join(ROOT_DIR, 'isolate.py'),
      mode,
      '--isolated', self.isolated,
      '-V', 'chromeos', str(chromeos_value),
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
      'isolate_smoke_test.isolated',
      'isolate_smoke_test.isolated.state',
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
      os.path.join('hashtable', calc_sha1(os.path.join(self.isolated))),
      'isolate_smoke_test.isolated',
      'isolate_smoke_test.isolated.state',
      os.path.join('root', 'tests', 'isolate', 'touch_root.isolate'),
      os.path.join('root', 'tests', 'isolate', 'touch_root.py'),
      os.path.join('root', 'isolate.py'),
    ])
    self.assertEquals(files, list_files_tree(self.tempdir))

  def test_remap(self):
    self._execute('remap', ['--isolate', self.filename()], False)
    files = sorted([
      'isolate_smoke_test.isolated',
      'isolate_smoke_test.isolated.state',
      os.path.join('root', 'tests', 'isolate', 'touch_root.isolate'),
      os.path.join('root', 'tests', 'isolate', 'touch_root.py'),
      os.path.join('root', 'isolate.py'),
    ])
    self.assertEquals(files, list_files_tree(self.tempdir))

  def test_run(self):
    self._execute('run', ['--isolate', self.filename()], False)
    files = sorted([
      'isolate_smoke_test.isolated',
      'isolate_smoke_test.isolated.state',
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
        unicode(os.path.join(
          self.root, 'tests', 'isolate', 'touch_root.isolate')))
    self.assertEquals(expected, output)
    # In theory the file is going to be updated but in practice its content
    # won't change.

    # Clean the directory from the logs, which are OS-specific.
    isolate.trace_inputs.get_api().clean_trace(
        os.path.join(self.tempdir, 'isolate_smoke_test.isolated.log'))
    files = sorted([
      'isolate_smoke_test.isolated',
      'isolate_smoke_test.isolated.state',
      os.path.join('root', 'tests', 'isolate', 'touch_root.isolate'),
      os.path.join('root', 'tests', 'isolate', 'touch_root.py'),
      os.path.join('root', 'isolate.py'),
    ])
    self.assertEquals(files, list_files_tree(self.tempdir))


if __name__ == '__main__':
  VERBOSE = '-v' in sys.argv
  logging.basicConfig(level=logging.DEBUG if VERBOSE else logging.ERROR)
  if VERBOSE:
    unittest.TestCase.maxDiff = None
  unittest.main()
