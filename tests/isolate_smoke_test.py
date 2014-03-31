#!/usr/bin/env python
# Copyright 2012 The Swarming Authors. All rights reserved.
# Use of this source code is governed under the Apache License, Version 2.0 that
# can be found in the LICENSE file.

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
import isolateserver
from utils import file_path
import trace_test_util
# Create shortcuts.
from isolate_format import KEY_TOUCHED, KEY_TRACKED, KEY_UNTRACKED


VERBOSE = False

ALGO = hashlib.sha1
HASH_NULL = ALGO().hexdigest()


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


def _isolate_dict_to_string(values):
  buf = cStringIO.StringIO()
  isolate.isolate_format.pretty_print(values, buf)
  return buf.getvalue()


def _wrap_in_condition(variables):
  """Wraps a variables dict inside the current OS condition.

  Returns the equivalent string.
  """
  return _isolate_dict_to_string(
      {
        'conditions': [
          ['OS=="mac" and chromeos==0', {
            'variables': variables
          }],
        ],
      })


def _fix_file_mode(filename, read_only):
  """4 modes are supported, 0750 (rwx), 0640 (rw), 0550 (rx), 0440 (r)."""
  min_mode = 0440
  if not read_only:
    min_mode |= 0200
  return (min_mode | 0110) if filename.endswith('.py') else min_mode


class Isolate(unittest.TestCase):
  def test_help_modes(self):
    # Check coherency in the help and implemented modes.
    p = subprocess.Popen(
        [sys.executable, os.path.join(ROOT_DIR, 'isolate.py'), '--help'],
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
        cwd=ROOT_DIR)
    out = p.communicate()[0].splitlines()
    self.assertEqual(0, p.returncode)
    out = out[out.index('Commands are:') + 1:]
    out = out[:out.index('')]
    regexp = '^  (?:\x1b\\[\\d\\dm|)(\\w+)\s*(:?\x1b\\[\\d\\dm|) .+'
    modes = [re.match(regexp, l) for l in out]
    modes = [m.group(1) for m in modes if m]
    EXPECTED_MODES = (
        'archive',
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
    # If a new command is added it should at least has a bare test.
    self.assertEqual(sorted(EXPECTED_MODES), sorted(modes))


class IsolateTempdir(unittest.TestCase):
  def setUp(self):
    super(IsolateTempdir, self).setUp()
    self.tempdir = tempfile.mkdtemp(prefix='isolate_smoke_')
    self.isolated = os.path.join(self.tempdir, 'isolate_smoke_test.isolated')

  def tearDown(self):
    try:
      logging.debug(self.tempdir)
      shutil.rmtree(self.tempdir)
    finally:
      super(IsolateTempdir, self).tearDown()

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
        if sys.platform != 'win32':
          v[u'm'] = _fix_file_mode(relfile, read_only)
      if with_time:
        # Used to skip recalculating the hash. Use the most recent update
        # time.
        v[u't'] = int(round(filestats.st_mtime))
      if is_link:
        v[u'l'] = os.readlink(filepath)  # pylint: disable=E1101
      else:
        # Upgrade the value to unicode so diffing the structure in case of
        # test failure is easier, since the basestring type must match,
        # str!=unicode.
        v[u'h'] = unicode(isolateserver.hash_file(filepath, ALGO))

    if empty_file:
      item = files[empty_file]
      item['h'] = unicode(HASH_NULL)
      if sys.platform != 'win32':
        item['m'] = 288
      item['s'] = 0
      if with_time:
        item['T'] = True
        item.pop('t', None)
    return files

  def _expected_isolated(self, args, read_only, empty_file):
    """Verifies self.isolated contains the expected data."""
    expected = {
      u'algo': u'sha-1',
      u'files': self._gen_files(read_only, empty_file, False),
      u'relative_cwd': unicode(RELATIVE_CWD[self.case()]),
      u'version': unicode(isolate.isolateserver.ISOLATED_FILE_VERSION),
    }
    if read_only is not None:
      expected[u'read_only'] = read_only
    if args:
      expected[u'command'] = [u'python'] + [unicode(x) for x in args]
    self.assertEqual(expected, json.load(open(self.isolated, 'r')))

  def _expected_saved_state(
      self, args, read_only, empty_file, extra_vars, root_dir):
    expected = {
      u'OS': unicode(sys.platform),
      u'algo': u'sha-1',
      u'child_isolated_files': [],
      u'command': [],
      u'config_variables': {
        u'OS': u'mac',
        u'chromeos': 0,
      },
      u'extra_variables': {
        u'EXECUTABLE_SUFFIX': u'.exe' if sys.platform == 'win32' else u'',
      },
      u'files': self._gen_files(read_only, empty_file, True),
      u'isolate_file': file_path.safe_relpath(
          file_path.get_native_path_case(unicode(self.filename())),
          unicode(os.path.dirname(self.isolated))),
      u'path_variables': {},
      u'relative_cwd': unicode(RELATIVE_CWD[self.case()]),
      u'root_dir': unicode(root_dir or os.path.dirname(self.filename())),
      u'version': unicode(isolate.SavedState.EXPECTED_VERSION),
    }
    if args:
      expected[u'command'] = [u'python'] + [unicode(x) for x in args]
    expected['extra_variables'].update(extra_vars or {})
    self.assertEqual(expected, json.load(open(self.saved_state(), 'r')))

  def _expect_results(
      self, args, read_only, extra_vars, empty_file, root_dir=None):
    self._expected_isolated(args, read_only, empty_file)
    self._expected_saved_state(
        args, read_only, empty_file, extra_vars, root_dir)
    # Also verifies run_isolated.py will be able to read it.
    with open(self.isolated, 'rb') as f:
      isolate.isolateserver.load_isolated(f.read(), ALGO)

  def _expect_no_result(self):
    self.assertFalse(os.path.exists(self.isolated))

  def _get_cmd(self, mode):
    return [
      sys.executable, os.path.join(ROOT_DIR, 'isolate.py'),
      mode,
      '--isolated', self.isolated,
      '--isolate', self.filename(),
      '--config-variable', 'OS', 'mac',
      '--config-variable', 'chromeos', '0',
    ]

  def _execute(self, mode, case, args, need_output, cwd=ROOT_DIR):
    """Executes isolate.py."""
    self.assertEqual(
        case,
        self.case() + '.isolate',
        'Rename the test case to test_%s()' % case)
    cmd = self._get_cmd(mode)
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

  def _test_all_items_invalid(self, mode):
    out = self._execute(mode, 'all_items_invalid.isolate',
                        ['--ignore_broken_item'], True)
    self._expect_results(['empty.py'], None, None, None)

    return out or ''

  def _test_missing_trailing_slash(self, mode):
    try:
      self._execute(mode, 'missing_trailing_slash.isolate', [], True)
      self.fail()
    except subprocess.CalledProcessError as e:
      self.assertEqual('', e.output)
      out = e.stderr
    self._expect_no_result()
    root = file_path.get_native_path_case(unicode(ROOT_DIR))
    expected = (
      'Input directory %s must have a trailing slash' %
          os.path.join(root, 'tests', 'isolate', 'files1')
    )
    self.assertIn(expected, out)

  def _test_non_existent(self, mode):
    try:
      self._execute(mode, 'non_existent.isolate', [], True)
      self.fail()
    except subprocess.CalledProcessError as e:
      self.assertEqual('', e.output)
      out = e.stderr
    self._expect_no_result()
    root = file_path.get_native_path_case(unicode(ROOT_DIR))
    expected = (
      'Input file %s doesn\'t exist' %
          os.path.join(root, 'tests', 'isolate', 'A_file_that_do_not_exist')
    )
    self.assertIn(expected, out)


class IsolateOutdir(IsolateTempdir):
  def setUp(self):
    super(IsolateOutdir, self).setUp()
    # The tests assume the current directory is the file's directory.
    os.chdir(ROOT_DIR)
    self.outdir = os.path.join(self.tempdir, 'isolated')

  def _expect_no_tree(self):
    # No outdir was created.
    self.assertFalse(os.path.exists(self.outdir))

  def _result_tree(self):
    return list_files_tree(self.outdir)

  def _expected_tree(self):
    """Verifies the files written in the temporary directory."""
    self.assertEqual(sorted(DEPENDENCIES[self.case()]), self._result_tree())

  def _get_cmd(self, mode):
    """Adds --outdir for the commands supporting it."""
    cmd = super(IsolateOutdir, self)._get_cmd(mode)
    cmd.extend(('--outdir', self.outdir))
    return cmd

  def _test_missing_trailing_slash(self, mode):
    super(IsolateOutdir, self)._test_missing_trailing_slash(mode)
    self._expect_no_tree()

  def _test_non_existent(self, mode):
    super(IsolateOutdir, self)._test_non_existent(mode)
    self._expect_no_tree()


class Isolate_check(IsolateTempdir):
  def test_fail(self):
    self._execute('check', 'fail.isolate', [], False)
    self._expect_results(['fail.py'], None, None, None)

  def test_missing_trailing_slash(self):
    self._test_missing_trailing_slash('check')

  def test_non_existent(self):
    self._test_non_existent('check')

  def test_all_items_invalid(self):
    out = self._test_all_items_invalid('check')
    self.assertEqual('', out)

  def test_no_run(self):
    self._execute('check', 'no_run.isolate', [], False)
    self._expect_results([], None, None, None)

  # TODO(csharp): Disabled until crbug.com/150823 is fixed.
  def do_not_test_touch_only(self):
    self._execute(
        'check', 'touch_only.isolate', ['--extra-variable', 'FLAG', 'gyp'],
        False)
    empty = os.path.join('files1', 'test_file1.txt')
    self._expected_isolated(['touch_only.py', 'gyp'], None, empty)

  def test_touch_root(self):
    self._execute('check', 'touch_root.isolate', [], False)
    self._expect_results(['touch_root.py'], None, None, None, ROOT_DIR)

  def test_with_flag(self):
    self._execute(
        'check', 'with_flag.isolate', ['--extra-variable', 'FLAG', 'gyp'],
        False)
    self._expect_results(
        ['with_flag.py', 'gyp'], None, {u'FLAG': u'gyp'}, None)

  if sys.platform != 'win32':
    def test_symlink_full(self):
      self._execute('check', 'symlink_full.isolate', [], False)
      self._expect_results(['symlink_full.py'], None, None, None)

    def test_symlink_partial(self):
      self._execute('check', 'symlink_partial.isolate', [], False)
      self._expect_results(['symlink_partial.py'], None, None, None)

    def test_symlink_outside_build_root(self):
      self._execute('check', 'symlink_outside_build_root.isolate', [], False)
      self._expect_results(['symlink_outside_build_root.py'], None, None, None)


class Isolate_hashtable(IsolateOutdir):
  def _gen_expected_tree(self, empty_file):
    expected = [
      unicode(v['h'])
      for v in self._gen_files(False, empty_file, False).itervalues()
    ]
    expected.append(
        unicode(isolateserver.hash_file(self.isolated, ALGO)))
    return expected

  def _expected_hash_tree(self, empty_file):
    """Verifies the files written in the temporary directory."""
    self.assertEqual(
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
    expected = (
        '%s  isolate_smoke_test.isolated\n' %
        isolateserver.hash_file(self.isolated, ALGO))
    self.assertEqual(expected, out)
    self._expected_hash_tree(None)

  def test_no_run(self):
    self._execute('hashtable', 'no_run.isolate', [], False)
    self._expected_hash_tree(None)
    self._expect_results([], None, None, None)

  def test_split(self):
    self._execute(
        'hashtable',
        'split.isolate',
        [
          '--path-variable', 'DEPTH', '.',
          '--path-variable', 'PRODUCT_DIR', 'files1',
        ],
        False,
        cwd=os.path.join(ROOT_DIR, 'tests', 'isolate'))
    # Reimplement _expected_hash_tree():
    tree = self._gen_expected_tree(None)
    isolated_base = self.isolated[:-len('.isolated')]
    isolated_hashes = [
      unicode(isolateserver.hash_file(isolated_base + '.0.isolated', ALGO)),
      unicode(isolateserver.hash_file(isolated_base + '.1.isolated', ALGO)),
    ]
    tree.extend(isolated_hashes)
    self.assertEqual(sorted(tree), map(unicode, self._result_tree()))

    # Reimplement _expected_isolated():
    files = self._gen_files(None, None, False)
    expected = {
      u'algo': u'sha-1',
      u'command': [u'python', u'split.py'],
      u'files': {u'split.py': files['split.py']},
      u'includes': isolated_hashes,
      u'relative_cwd': unicode(RELATIVE_CWD[self.case()]),
      u'version': unicode(isolate.isolateserver.ISOLATED_FILE_VERSION),
    }
    self.assertEqual(expected, json.load(open(self.isolated, 'r')))

    key = os.path.join(u'test', 'data', 'foo.txt')
    expected = {
      u'algo': u'sha-1',
      u'files': {key: files[key]},
      u'version': unicode(isolate.isolateserver.ISOLATED_FILE_VERSION),
    }
    self.assertEqual(
        expected, json.load(open(isolated_base + '.0.isolated', 'r')))

    key = os.path.join(u'files1', 'subdir', '42.txt')
    expected = {
      u'algo': u'sha-1',
      u'files': {key: files[key]},
      u'version': unicode(isolate.isolateserver.ISOLATED_FILE_VERSION),
    }
    self.assertEqual(
        expected, json.load(open(isolated_base + '.1.isolated', 'r')))

  # TODO(csharp): Disabled until crbug.com/150823 is fixed.
  def do_not_test_touch_only(self):
    self._execute(
        'hashtable', 'touch_only.isolate', ['--extra-variable', 'FLAG', 'gyp'],
        False)
    empty = os.path.join('files1', 'test_file1.txt')
    self._expected_hash_tree(empty)
    self._expected_isolated(['touch_only.py', 'gyp'], None, empty)

  def test_touch_root(self):
    self._execute('hashtable', 'touch_root.isolate', [], False)
    self._expected_hash_tree(None)
    self._expect_results(['touch_root.py'], None, None, None, ROOT_DIR)

  def test_with_flag(self):
    self._execute(
        'hashtable', 'with_flag.isolate', ['--extra-variable', 'FLAG', 'gyp'],
        False)
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
      expected.append(isolateserver.hash_file(self.isolated, ALGO))
      self.assertEqual(sorted(expected), self._result_tree())
      self._expect_results(['symlink_full.py'], None, None, None)

    def test_symlink_partial(self):
      self._execute('hashtable', 'symlink_partial.isolate', [], False)
      # Construct our own tree.
      expected = [
        str(v['h'])
        for v in self._gen_files(False, None, False).itervalues() if 'h' in v
      ]
      expected.append(isolateserver.hash_file(self.isolated, ALGO))
      self.assertEqual(sorted(expected), self._result_tree())
      self._expect_results(['symlink_partial.py'], None, None, None)

    def test_symlink_outside_build_root(self):
      self._execute(
          'hashtable', 'symlink_outside_build_root.isolate', [], False)
      # Construct our own tree.
      expected = [
        str(v['h'])
        for v in self._gen_files(False, None, False).itervalues() if 'h' in v
      ]
      expected.append(isolateserver.hash_file(self.isolated, ALGO))
      self.assertEqual(sorted(expected), self._result_tree())
      self._expect_results(['symlink_outside_build_root.py'], None, None, None)


class Isolate_remap(IsolateOutdir):
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
    self._execute(
        'remap', 'touch_only.isolate', ['--extra-variable', 'FLAG', 'gyp'],
        False)
    self._expected_tree()
    empty = os.path.join('files1', 'test_file1.txt')
    self._expect_results(
        ['touch_only.py', 'gyp'], None, {u'FLAG': u'gyp'}, empty)

  def test_touch_root(self):
    self._execute('remap', 'touch_root.isolate', [], False)
    self._expected_tree()
    self._expect_results(['touch_root.py'], None, None, None, ROOT_DIR)

  def test_with_flag(self):
    self._execute(
        'remap', 'with_flag.isolate', ['--extra-variable', 'FLAG', 'gyp'],
        False)
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


class Isolate_run(IsolateTempdir):
  def test_fail(self):
    try:
      self._execute('run', 'fail.isolate', [], False)
      self.fail()
    except subprocess.CalledProcessError:
      pass
    self._expect_results(['fail.py'], None, None, None)

  def test_missing_trailing_slash(self):
    self._test_missing_trailing_slash('run')

  def test_non_existent(self):
    self._test_non_existent('run')

  def test_all_items_invalid(self):
    out = self._test_all_items_invalid('run')
    self.assertEqual('', out)

  def test_no_run(self):
    try:
      self._execute('run', 'no_run.isolate', [], False)
      self.fail()
    except subprocess.CalledProcessError:
      pass
    self._expect_no_result()

  # TODO(csharp): Disabled until crbug.com/150823 is fixed.
  def do_not_test_touch_only(self):
    self._execute(
        'run', 'touch_only.isolate', ['--extra-variable', 'FLAG', 'run'],
        False)
    empty = os.path.join('files1', 'test_file1.txt')
    self._expect_results(
        ['touch_only.py', 'run'], None, {u'FLAG': u'run'}, empty)

  def test_touch_root(self):
    self._execute('run', 'touch_root.isolate', [], False)
    self._expect_results(['touch_root.py'], None, None, None, ROOT_DIR)

  def test_with_flag(self):
    self._execute(
        'run', 'with_flag.isolate', ['--extra-variable', 'FLAG', 'run'],
        False)
    self._expect_results(
        ['with_flag.py', 'run'], None, {u'FLAG': u'run'}, None)

  if sys.platform != 'win32':
    def test_symlink_full(self):
      self._execute('run', 'symlink_full.isolate', [], False)
      self._expect_results(['symlink_full.py'], None, None, None)

    def test_symlink_partial(self):
      self._execute('run', 'symlink_partial.isolate', [], False)
      self._expect_results(['symlink_partial.py'], None, None, None)

    def test_symlink_outside_build_root(self):
      self._execute('run', 'symlink_outside_build_root.isolate', [], False)
      self._expect_results(['symlink_outside_build_root.py'], None, None, None)


class Isolate_trace_read_merge(IsolateTempdir):
  # Tests both trace, read and merge.
  # Warning: merge updates .isolate files. But they are currently in their
  # canonical format so they shouldn't be changed.
  def _check_merge(self, filename):
    filepath = file_path.get_native_path_case(
            os.path.join(unicode(ROOT_DIR), 'tests', 'isolate', filename))
    expected = 'Updating %s\n' % file_path.safe_relpath(filepath, self.tempdir)
    with open(filepath, 'rb') as f:
      old_content = f.read()
    out = self._execute('merge', filename, [], True) or ''
    self.assertEqual(expected, out)
    with open(filepath, 'rb') as f:
      new_content = f.read()
    self.assertEqual(old_content, new_content)

  @trace_test_util.check_can_trace
  def test_fail(self):
    # Even if the process returns non-zero, the trace will still be good.
    try:
      self._execute('trace', 'fail.isolate', ['-v'], True)
      self.fail()
    except subprocess.CalledProcessError, e:
      self.assertEqual('', e.output)
    self._expect_results(['fail.py'], None, None, None)
    expected = _wrap_in_condition(
        {
          KEY_TRACKED: ['fail.py'],
        })
    out = self._execute('read', 'fail.isolate', [], True) or ''
    self.assertEqual(expected.splitlines(), out.splitlines())
    self._check_merge('fail.isolate')

  def test_missing_trailing_slash(self):
    self._test_missing_trailing_slash('trace')

  def test_non_existent(self):
    self._test_non_existent('trace')

  @trace_test_util.check_can_trace
  def test_all_items_invalid(self):
    out = self._test_all_items_invalid('trace')
    self.assertEqual('', out)

  def test_no_run(self):
    try:
      self._execute('trace', 'no_run.isolate', [], True)
      self.fail()
    except subprocess.CalledProcessError, e:
      out = e.output
      err = e.stderr
    self._expect_no_result()
    expected = 'No command to run.'
    self.assertEqual('', out)
    self.assertIn(expected, err)

  # TODO(csharp): Disabled until crbug.com/150823 is fixed.
  def do_not_test_touch_only(self):
    out = self._execute(
        'trace', 'touch_only.isolate', ['--extra-variable', 'FLAG', 'trace'],
        True)
    self.assertEqual('', out)
    empty = os.path.join('files1', 'test_file1.txt')
    self._expect_results(
        ['touch_only.py', 'trace'], None, {u'FLAG': u'trace'}, empty)
    expected = {
      KEY_TRACKED: ['touch_only.py'],
      # Note that .isolate format mandates / and not os.path.sep.
      KEY_TOUCHED: ['files1/test_file1.txt'],
    }
    if sys.platform != 'linux2':
      # TODO(maruel): Implement touch-only tracing on non-linux.
      del expected[KEY_TOUCHED]

    out = self._execute('read', 'touch_only.isolate', [], True)
    self.assertEqual(_wrap_in_condition(expected), out)
    self._check_merge('touch_only.isolate')

  @trace_test_util.check_can_trace
  def test_touch_root(self):
    out = self._execute('trace', 'touch_root.isolate', [], True)
    self.assertEqual('', out)
    self._expect_results(['touch_root.py'], None, None, None, ROOT_DIR)
    expected = _wrap_in_condition(
        {
          KEY_TRACKED: [
            '../../isolate.py',
            'touch_root.py',
          ],
        })
    out = self._execute('read', 'touch_root.isolate', [], True)
    self.assertEqual(expected, out)
    self._check_merge('touch_root.isolate')

  @trace_test_util.check_can_trace
  def test_with_flag(self):
    out = self._execute(
        'trace', 'with_flag.isolate', ['--extra-variable', 'FLAG', 'trace'],
        True)
    self.assertEqual('', out)
    self._expect_results(
        ['with_flag.py', 'trace'], None, {u'FLAG': u'trace'}, None)
    expected = {
      KEY_TRACKED: ['with_flag.py'],
      # Note that .isolate format mandates / and not os.path.sep.
      KEY_UNTRACKED: ['files1/'],
    }
    out = self._execute('read', 'with_flag.isolate', [], True)
    self.assertEqual(_wrap_in_condition(expected), out)
    self._check_merge('with_flag.isolate')

  if sys.platform != 'win32':
    def test_symlink_full(self):
      out = self._execute(
          'trace', 'symlink_full.isolate', [], True)
      self.assertEqual('', out)
      self._expect_results(['symlink_full.py'], None, None, None)
      expected = {
        KEY_TRACKED: ['symlink_full.py'],
        # Note that .isolate format mandates / and not os.path.sep.
        KEY_UNTRACKED: ['files2/'],
      }
      out = self._execute('read', 'symlink_full.isolate', [], True)
      self.assertEqual(_wrap_in_condition(expected), out)
      self._check_merge('symlink_full.isolate')

    def test_symlink_partial(self):
      out = self._execute(
          'trace', 'symlink_partial.isolate', [], True)
      self.assertEqual('', out)
      self._expect_results(['symlink_partial.py'], None, None, None)
      expected = {
        KEY_TRACKED: ['symlink_partial.py'],
        KEY_UNTRACKED: ['files2/test_file2.txt'],
      }
      out = self._execute('read', 'symlink_partial.isolate', [], True)
      self.assertEqual(_wrap_in_condition(expected), out)
      self._check_merge('symlink_partial.isolate')

    def test_symlink_outside_build_root(self):
      out = self._execute(
          'trace', 'symlink_outside_build_root.isolate', [], True)
      self.assertEqual('', out)
      self._expect_results(['symlink_outside_build_root.py'], None, None, None)
      expected = {
        KEY_TRACKED: ['symlink_outside_build_root.py'],
        KEY_UNTRACKED: ['link_outside_build_root/'],
      }
      out = self._execute(
          'read', 'symlink_outside_build_root.isolate', [], True)
      self.assertEqual(_wrap_in_condition(expected), out)
      self._check_merge('symlink_outside_build_root.isolate')


class IsolateNoOutdir(IsolateTempdir):
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

  def _execute(self, mode, args, need_output):  # pylint: disable=W0221
    """Executes isolate.py."""
    cmd = [
      sys.executable, os.path.join(ROOT_DIR, 'isolate.py'),
      mode,
      '--isolated', self.isolated,
      '--config-variable', 'OS', 'mac',
      '--config-variable', 'chromeos', '0',
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
    self.assertEqual(3, len(test_id))
    self.assertEqual('__main__', test_id[0])
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
    self.assertEqual(files, list_files_tree(self.tempdir))

  def test_hashtable(self):
    with self.assertRaises(CalledProcessError):
      self._execute('hashtable', ['--isolate', self.filename()], False)

  def test_remap(self):
    with self.assertRaises(CalledProcessError):
      self._execute('remap', ['--isolate', self.filename()], False)

  def test_run(self):
    self._execute('run', ['--isolate', self.filename()], False)
    files = sorted([
      'isolate_smoke_test.isolated',
      'isolate_smoke_test.isolated.state',
      os.path.join('root', 'tests', 'isolate', 'touch_root.isolate'),
      os.path.join('root', 'tests', 'isolate', 'touch_root.py'),
      os.path.join('root', 'isolate.py'),
    ])
    self.assertEqual(files, list_files_tree(self.tempdir))

  @trace_test_util.check_can_trace
  def test_trace_read_merge(self):
    self._execute('trace', ['--isolate', self.filename()], False)
    # Read the trace before cleaning up. No need to specify self.filename()
    # because add the needed information is in the .state file.
    output = self._execute('read', [], True)
    expected = {
      KEY_TRACKED: [
        '../../isolate.py',
        'touch_root.py',
      ],
    }
    self.assertEqual(_wrap_in_condition(expected), output)

    output = self._execute('merge', [], True)
    expected = 'Updating %s\n' % os.path.join(
        'root', 'tests', 'isolate', 'touch_root.isolate')
    self.assertEqual(expected, output)
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
    self.assertEqual(files, list_files_tree(self.tempdir))


class IsolateOther(IsolateTempdir):
  def test_run_mixed(self):
    # Test when a user mapped from a directory and then replay from another
    # directory. This is a very rare corner case.
    indir = os.path.join(self.tempdir, 'input')
    os.mkdir(indir)
    for i in ('simple.py', 'simple.isolate'):
      shutil.copy(
          os.path.join(ROOT_DIR, 'tests', 'isolate', i),
          os.path.join(indir, i))
    proc = subprocess.Popen(
        [
          sys.executable, 'isolate.py',
          'check',
          '-i', os.path.join(indir, 'simple.isolate'),
          '-s', os.path.join(indir, 'simple.isolated'),
          '--config-variable', 'OS', 'mac',
        ],
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
        cwd=ROOT_DIR)
    stdout = proc.communicate()[0]
    self.assertEqual('', stdout)
    self.assertEqual(0, proc.returncode)
    expected = [
      'simple.isolate', 'simple.isolated', 'simple.isolated.state', 'simple.py',
    ]
    self.assertEqual(expected, sorted(os.listdir(indir)))

    # Remove the original directory.
    indir2 = indir + '2'
    os.rename(indir, indir2)

    # simple.isolated.state is required; it contains the variables.
    proc = subprocess.Popen(
        [
          sys.executable, 'isolate.py', 'run',
          '-s', os.path.join(indir2, 'simple.isolated'),
          '--skip-refresh',
        ],
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
        cwd=ROOT_DIR,
        universal_newlines=True)
    stdout = proc.communicate()[0]
    self.assertEqual(1, proc.returncode)
    self.assertTrue('simple.py is missing' in stdout)

  def test_empty_and_renamed(self):
    a_isolate = os.path.join(self.tempdir, 'a.isolate')
    with open(a_isolate, 'wb') as f:
      f.write('{}')

    cmd = [
        sys.executable, 'isolate.py', 'check',
        '-s', os.path.join(self.tempdir, 'out.isolated'),
    ]
    subprocess.check_call(cmd + ['-i', a_isolate])

    # Move the .isolate file aside and rerun the command with the new source but
    # same destination.
    b_isolate = os.path.join(self.tempdir, 'b.isolate')
    os.rename(a_isolate, b_isolate)
    subprocess.check_call(cmd + ['-i', b_isolate])


if __name__ == '__main__':
  VERBOSE = '-v' in sys.argv
  logging.basicConfig(level=logging.DEBUG if VERBOSE else logging.ERROR)
  if VERBOSE:
    unittest.TestCase.maxDiff = None
  unittest.main()
