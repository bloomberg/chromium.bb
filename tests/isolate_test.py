#!/usr/bin/env python
# Copyright 2012 The Swarming Authors. All rights reserved.
# Use of this source code is governed under the Apache License, Version 2.0 that
# can be found in the LICENSE file.

import cStringIO
import hashlib
import json
import logging
import os
import sys
import tempfile
import unittest

ROOT_DIR = unicode(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
sys.path.insert(0, ROOT_DIR)
sys.path.insert(0, os.path.join(ROOT_DIR, 'third_party'))

from depot_tools import auto_stub
import isolate
from utils import file_path
from utils import tools


ALGO = hashlib.sha1


def _size(*args):
  return os.stat(os.path.join(ROOT_DIR, *args)).st_size


def hash_file(*args):
  return isolate.isolateserver.hash_file(os.path.join(ROOT_DIR, *args), ALGO)


class IsolateBase(auto_stub.TestCase):
  def setUp(self):
    super(IsolateBase, self).setUp()
    self.old_cwd = os.getcwd()
    self.cwd = tempfile.mkdtemp(prefix='isolate_')
    # Everything should work even from another directory.
    os.chdir(self.cwd)

  def tearDown(self):
    try:
      os.chdir(self.old_cwd)
      isolate.run_isolated.rmtree(self.cwd)
    finally:
      super(IsolateBase, self).tearDown()


class IsolateTest(IsolateBase):
  def test_savedstate_load_minimal(self):
    # The file referenced by 'isolate_file' must exist even if its content is
    # not read.
    open(os.path.join(self.cwd, 'fake.isolate'), 'wb').close()
    values = {
      'isolate_file': 'fake.isolate',
    }
    expected = {
      'algo': 'sha-1',
      'child_isolated_files': [],
      'config_variables': {},
      'command': [],
      'extra_variables': {},
      'files': {},
      'isolate_file': 'fake.isolate',
      'path_variables': {},
      'version': isolate.isolateserver.ISOLATED_FILE_VERSION,
    }
    saved_state = isolate.SavedState.load(values, self.cwd)
    self.assertEqual(expected, saved_state.flatten())

  def test_savedstate_load(self):
    # The file referenced by 'isolate_file' must exist even if its content is
    # not read.
    open(os.path.join(self.cwd, 'fake.isolate'), 'wb').close()
    values = {
      'config_variables': {
        'OS': isolate.get_flavor(),
      },
      'extra_variables': {
        'foo': 42,
      },
      'isolate_file': 'fake.isolate',
    }
    expected = {
      'algo': 'sha-1',
      'child_isolated_files': [],
      'command': [],
      'config_variables': {
        'OS': isolate.get_flavor(),
      },
      'extra_variables': {
        'foo': 42,
      },
      'files': {},
      'isolate_file': 'fake.isolate',
      'path_variables': {},
      'version': isolate.isolateserver.ISOLATED_FILE_VERSION,
    }
    saved_state = isolate.SavedState.load(values, self.cwd)
    self.assertEqual(expected, saved_state.flatten())

  def test_variable_arg(self):
    parser = isolate.OptionParserIsolate()
    parser.require_isolated = False
    expected_path = {
      'Baz': 'sub=string',
    }
    expected_config = {
      'Foo': 'bar',
      'OS': isolate.get_flavor(),
    }
    expected_extra = {
      'biz': 'b uz=a',
      'EXECUTABLE_SUFFIX': '.exe' if isolate.get_flavor() == 'win' else '',
    }

    options, args = parser.parse_args(
        ['--config-variable', 'Foo', 'bar',
          '--path-variable', 'Baz=sub=string',
          '--extra-variable', 'biz', 'b uz=a'])
    self.assertEqual(expected_path, options.path_variables)
    self.assertEqual(expected_config, options.config_variables)
    self.assertEqual(expected_extra, options.extra_variables)
    self.assertEqual([], args)

  def test_variable_arg_fail(self):
    parser = isolate.OptionParserIsolate()
    self.mock(sys, 'stderr', cStringIO.StringIO())
    with self.assertRaises(SystemExit):
      parser.parse_args(['--config-variable', 'Foo'])

  def test_blacklist(self):
    ok = [
      '.git2',
      '.pyc',
      '.swp',
      'allo.git',
      'foo',
    ]
    blocked = [
      '.git',
      os.path.join('foo', '.git'),
      'foo.pyc',
      'bar.swp',
    ]
    blacklist = tools.gen_blacklist(isolate.isolateserver.DEFAULT_BLACKLIST)
    for i in ok:
      self.assertFalse(blacklist(i), i)
    for i in blocked:
      self.assertTrue(blacklist(i), i)

  def test_blacklist_chromium(self):
    ok = [
      '.run_test_cases',
      'testserver.log2',
    ]
    blocked = [
      'foo.run_test_cases',
      'testserver.log',
      os.path.join('foo', 'testserver.log'),
    ]
    blacklist = tools.gen_blacklist(isolate.isolateserver.DEFAULT_BLACKLIST)
    for i in ok:
      self.assertFalse(blacklist(i), i)
    for i in blocked:
      self.assertTrue(blacklist(i), i)


class IsolateLoad(IsolateBase):
  def setUp(self):
    super(IsolateLoad, self).setUp()
    self.directory = tempfile.mkdtemp(prefix='isolate_')

  def tearDown(self):
    try:
      isolate.run_isolated.rmtree(self.directory)
    finally:
      super(IsolateLoad, self).tearDown()

  def _get_option(self, isolate_file):
    OS = isolate.get_flavor()
    chromeos_value = int(OS == 'linux')
    class Options(object):
      isolated = os.path.join(self.directory, 'foo.isolated')
      outdir = os.path.join(self.directory, 'outdir')
      isolate = isolate_file
      path_variables = {}
      config_variables = {'OS': OS, 'chromeos': chromeos_value}
      extra_variables = {'foo': 'bar'}
      ignore_broken_items = False
    return Options()

  def _cleanup_isolated(self, expected_isolated):
    """Modifies isolated to remove the non-deterministic parts."""
    if sys.platform == 'win32':
      # 'm' are not saved in windows.
      for values in expected_isolated['files'].itervalues():
        self.assertTrue(values.pop('m'))

  def _cleanup_saved_state(self, actual_saved_state):
    for item in actual_saved_state['files'].itervalues():
      self.assertTrue(item.pop('t'))

  def test_load_stale_isolated(self):
    isolate_file = os.path.join(
        ROOT_DIR, 'tests', 'isolate', 'touch_root.isolate')

    # Data to be loaded in the .isolated file. Do not create a .state file.
    input_data = {
      'command': ['python'],
      'files': {
        'foo': {
          "m": 416,
          "h": "invalid",
          "s": 538,
          "t": 1335146921,
        },
        os.path.join('tests', 'isolate', 'touch_root.py'): {
          "m": 488,
          "h": "invalid",
          "s": 538,
          "t": 1335146921,
        },
      },
    }
    options = self._get_option(isolate_file)
    tools.write_json(options.isolated, input_data, False)

    # A CompleteState object contains two parts:
    # - Result instance stored in complete_state.isolated, corresponding to the
    #   .isolated file, is what is read by run_test_from_archive.py.
    # - SavedState instance stored in compelte_state.saved_state,
    #   corresponding to the .state file, which is simply to aid the developer
    #   when re-running the same command multiple times and contain
    #   discardable information.
    complete_state = isolate.load_complete_state(options, self.cwd, None, False)
    actual_isolated = complete_state.saved_state.to_isolated()
    actual_saved_state = complete_state.saved_state.flatten()

    expected_isolated = {
      'algo': 'sha-1',
      'command': ['python', 'touch_root.py'],
      'files': {
        os.path.join(u'tests', 'isolate', 'touch_root.py'): {
          'm': 488,
          'h': hash_file('tests', 'isolate', 'touch_root.py'),
          's': _size('tests', 'isolate', 'touch_root.py'),
        },
        u'isolate.py': {
          'm': 488,
          'h': hash_file('isolate.py'),
          's': _size('isolate.py'),
        },
      },
      'os': isolate.get_flavor(),
      'relative_cwd': os.path.join(u'tests', 'isolate'),
      'version': isolate.isolateserver.ISOLATED_FILE_VERSION,
    }
    self._cleanup_isolated(expected_isolated)
    self.assertEqual(expected_isolated, actual_isolated)

    expected_saved_state = {
      'algo': 'sha-1',
      'child_isolated_files': [],
      'command': ['python', 'touch_root.py'],
      'config_variables': {
        'OS': isolate.get_flavor(),
        'chromeos': options.config_variables['chromeos'],
      },
      'extra_variables': {
        'foo': 'bar',
      },
      'files': {
        os.path.join(u'tests', 'isolate', 'touch_root.py'): {
          'm': 488,
          'h': hash_file('tests', 'isolate', 'touch_root.py'),
          's': _size('tests', 'isolate', 'touch_root.py'),
        },
        u'isolate.py': {
          'm': 488,
          'h': hash_file('isolate.py'),
          's': _size('isolate.py'),
        },
      },
      'isolate_file': file_path.safe_relpath(
          file_path.get_native_path_case(isolate_file),
          os.path.dirname(options.isolated)),
      'relative_cwd': os.path.join(u'tests', 'isolate'),
      'path_variables': {},
      'version': isolate.isolateserver.ISOLATED_FILE_VERSION,
    }
    self._cleanup_isolated(expected_saved_state)
    self._cleanup_saved_state(actual_saved_state)
    self.assertEqual(expected_saved_state, actual_saved_state)

  def test_subdir(self):
    # The resulting .isolated file will be missing ../../isolate.py. It is
    # because this file is outside the --subdir parameter.
    isolate_file = os.path.join(
        ROOT_DIR, 'tests', 'isolate', 'touch_root.isolate')
    options = self._get_option(isolate_file)
    chromeos_value = int(isolate.get_flavor() == 'linux')
    options.config_variables['chromeos'] = chromeos_value
    complete_state = isolate.load_complete_state(
        options, self.cwd, os.path.join('tests', 'isolate'), False)
    actual_isolated = complete_state.saved_state.to_isolated()
    actual_saved_state = complete_state.saved_state.flatten()

    expected_isolated =  {
      'algo': 'sha-1',
      'command': ['python', 'touch_root.py'],
      'files': {
        os.path.join(u'tests', 'isolate', 'touch_root.py'): {
          'm': 488,
          'h': hash_file('tests', 'isolate', 'touch_root.py'),
          's': _size('tests', 'isolate', 'touch_root.py'),
        },
      },
      'os': isolate.get_flavor(),
      'relative_cwd': os.path.join(u'tests', 'isolate'),
      'version': isolate.isolateserver.ISOLATED_FILE_VERSION,
    }
    self._cleanup_isolated(expected_isolated)
    self.assertEqual(expected_isolated, actual_isolated)

    expected_saved_state = {
      'algo': 'sha-1',
      'child_isolated_files': [],
      'command': ['python', 'touch_root.py'],
      'config_variables': {
        'OS': isolate.get_flavor(),
        'chromeos': chromeos_value,
      },
      'extra_variables': {
        'foo': 'bar',
      },
      'files': {
        os.path.join(u'tests', 'isolate', 'touch_root.py'): {
          'm': 488,
          'h': hash_file('tests', 'isolate', 'touch_root.py'),
          's': _size('tests', 'isolate', 'touch_root.py'),
        },
      },
      'isolate_file': file_path.safe_relpath(
          file_path.get_native_path_case(isolate_file),
          os.path.dirname(options.isolated)),
      'relative_cwd': os.path.join(u'tests', 'isolate'),
      'path_variables': {},
      'version': isolate.isolateserver.ISOLATED_FILE_VERSION,
    }
    self._cleanup_isolated(expected_saved_state)
    self._cleanup_saved_state(actual_saved_state)
    self.assertEqual(expected_saved_state, actual_saved_state)

  def test_subdir_variable(self):
    # the resulting .isolated file will be missing ../../isolate.py. it is
    # because this file is outside the --subdir parameter.
    isolate_file = os.path.join(
        ROOT_DIR, 'tests', 'isolate', 'touch_root.isolate')
    options = self._get_option(isolate_file)
    chromeos_value = int(isolate.get_flavor() == 'linux')
    options.config_variables['chromeos'] = chromeos_value
    # Path variables are keyed on the directory containing the .isolate file.
    options.path_variables['TEST_ISOLATE'] = '.'
    # Note that options.isolated is in self.directory, which is a temporary
    # directory.
    complete_state = isolate.load_complete_state(
        options, os.path.join(ROOT_DIR, 'tests', 'isolate'),
        '<(TEST_ISOLATE)', False)
    actual_isolated = complete_state.saved_state.to_isolated()
    actual_saved_state = complete_state.saved_state.flatten()

    expected_isolated =  {
      'algo': 'sha-1',
      'command': ['python', 'touch_root.py'],
      'files': {
        os.path.join('tests', 'isolate', 'touch_root.py'): {
          'm': 488,
          'h': hash_file('tests', 'isolate', 'touch_root.py'),
          's': _size('tests', 'isolate', 'touch_root.py'),
        },
      },
      'os': isolate.get_flavor(),
      'relative_cwd': os.path.join(u'tests', 'isolate'),
      'version': isolate.isolateserver.ISOLATED_FILE_VERSION,
    }
    self._cleanup_isolated(expected_isolated)
    self.assertEqual(expected_isolated, actual_isolated)

    # It is important to note:
    # - the root directory is ROOT_DIR.
    # - relative_cwd is tests/isolate.
    # - TEST_ISOLATE is based of relative_cwd, so it represents tests/isolate.
    # - anything outside TEST_ISOLATE was not included in the 'files' section.
    expected_saved_state = {
      'algo': 'sha-1',
      'child_isolated_files': [],
      'command': ['python', 'touch_root.py'],
      'config_variables': {
        'OS': isolate.get_flavor(),
        'chromeos': chromeos_value,
      },
      'extra_variables': {
        'foo': 'bar',
      },
      'files': {
        os.path.join(u'tests', 'isolate', 'touch_root.py'): {
          'm': 488,
          'h': hash_file('tests', 'isolate', 'touch_root.py'),
          's': _size('tests', 'isolate', 'touch_root.py'),
        },
      },
      'isolate_file': file_path.safe_relpath(
          file_path.get_native_path_case(isolate_file),
          os.path.dirname(options.isolated)),
      'relative_cwd': os.path.join(u'tests', 'isolate'),
      'path_variables': {
        'TEST_ISOLATE': '.',
      },
      'version': isolate.isolateserver.ISOLATED_FILE_VERSION,
    }
    self._cleanup_isolated(expected_saved_state)
    self._cleanup_saved_state(actual_saved_state)
    self.assertEqual(expected_saved_state, actual_saved_state)

  def test_variable_not_exist(self):
    isolate_file = os.path.join(
        ROOT_DIR, 'tests', 'isolate', 'touch_root.isolate')
    options = self._get_option(isolate_file)
    options.path_variables['PRODUCT_DIR'] = os.path.join(u'tests', u'isolate')
    native_cwd = file_path.get_native_path_case(unicode(self.cwd))
    try:
      isolate.load_complete_state(options, self.cwd, None, False)
      self.fail()
    except isolate.ExecutionError, e:
      self.assertEqual(
          'PRODUCT_DIR=%s is not a directory' %
            os.path.join(native_cwd, 'tests', 'isolate'),
          e.args[0])

  def test_variable(self):
    isolate_file = os.path.join(
        ROOT_DIR, 'tests', 'isolate', 'touch_root.isolate')
    options = self._get_option(isolate_file)
    chromeos_value = int(isolate.get_flavor() == 'linux')
    options.config_variables['chromeos'] = chromeos_value
    options.path_variables['PRODUCT_DIR'] = os.path.join('tests', 'isolate')
    complete_state = isolate.load_complete_state(options, ROOT_DIR, None, False)
    actual_isolated = complete_state.saved_state.to_isolated()
    actual_saved_state = complete_state.saved_state.flatten()

    expected_isolated =  {
      'algo': 'sha-1',
      'command': ['python', 'touch_root.py'],
      'files': {
        u'isolate.py': {
          'm': 488,
          'h': hash_file('isolate.py'),
          's': _size('isolate.py'),
        },
        os.path.join(u'tests', 'isolate', 'touch_root.py'): {
          'm': 488,
          'h': hash_file('tests', 'isolate', 'touch_root.py'),
          's': _size('tests', 'isolate', 'touch_root.py'),
        },
      },
      'os': isolate.get_flavor(),
      'relative_cwd': os.path.join(u'tests', 'isolate'),
      'version': isolate.isolateserver.ISOLATED_FILE_VERSION,
    }
    self._cleanup_isolated(expected_isolated)
    self.assertEqual(expected_isolated, actual_isolated)

    expected_saved_state = {
      'algo': 'sha-1',
      'child_isolated_files': [],
      'command': ['python', 'touch_root.py'],
      'config_variables': {
        'OS': isolate.get_flavor(),
        'chromeos': chromeos_value,
      },
      'extra_variables': {
        'foo': 'bar',
      },
      'files': {
        u'isolate.py': {
          'm': 488,
          'h': hash_file('isolate.py'),
          's': _size('isolate.py'),
        },
        os.path.join(u'tests', 'isolate', 'touch_root.py'): {
          'm': 488,
          'h': hash_file('tests', 'isolate', 'touch_root.py'),
          's': _size('tests', 'isolate', 'touch_root.py'),
        },
      },
      'isolate_file': file_path.safe_relpath(
          file_path.get_native_path_case(isolate_file),
          os.path.dirname(options.isolated)),
      'relative_cwd': os.path.join(u'tests', 'isolate'),
      'path_variables': {
        'PRODUCT_DIR': '.',
      },
      'version': isolate.isolateserver.ISOLATED_FILE_VERSION,
    }
    self._cleanup_isolated(expected_saved_state)
    self._cleanup_saved_state(actual_saved_state)
    self.assertEqual(expected_saved_state, actual_saved_state)
    self.assertEqual([], os.listdir(self.directory))

  def test_root_dir_because_of_variable(self):
    # Ensures that load_isolate() works even when path variables have deep root
    # dirs. The end result is similar to touch_root.isolate, except that
    # no_run.isolate doesn't reference '..' at all.
    #
    # A real world example would be PRODUCT_DIR=../../out/Release but nothing in
    # this directory is mapped.
    #
    # Imagine base/base_unittests.isolate would not map anything in
    # PRODUCT_DIR. In that case, the automatically determined root dir is
    # src/base, since nothing outside this directory is mapped.
    isolate_file = os.path.join(
        ROOT_DIR, 'tests', 'isolate', 'no_run.isolate')
    options = self._get_option(isolate_file)
    chromeos_value = int(isolate.get_flavor() == 'linux')
    # Any directory outside ROOT_DIR/tests/isolate.
    options.path_variables['PRODUCT_DIR'] = os.path.join('third_party')
    complete_state = isolate.load_complete_state(options, ROOT_DIR, None, False)
    actual_isolated = complete_state.saved_state.to_isolated()
    actual_saved_state = complete_state.saved_state.flatten()

    expected_isolated = {
      'algo': 'sha-1',
      'files': {
        os.path.join(u'tests', 'isolate', 'files1', 'subdir', '42.txt'): {
          'm': 416,
          'h': hash_file('tests', 'isolate', 'files1', 'subdir', '42.txt'),
          's': _size('tests', 'isolate', 'files1', 'subdir', '42.txt'),
        },
        os.path.join(u'tests', 'isolate', 'files1', 'test_file1.txt'): {
          'm': 416,
          'h': hash_file('tests', 'isolate', 'files1', 'test_file1.txt'),
          's': _size('tests', 'isolate', 'files1', 'test_file1.txt'),
        },
        os.path.join(u'tests', 'isolate', 'files1', 'test_file2.txt'): {
          'm': 416,
          'h': hash_file('tests', 'isolate', 'files1', 'test_file2.txt'),
          's': _size('tests', 'isolate', 'files1', 'test_file2.txt'),
        },
        os.path.join(u'tests', 'isolate', 'no_run.isolate'): {
          'm': 416,
          'h': hash_file('tests', 'isolate', 'no_run.isolate'),
          's': _size('tests', 'isolate', 'no_run.isolate'),
        },
      },
      'os': isolate.get_flavor(),
      'relative_cwd': os.path.join(u'tests', 'isolate'),
      'version': isolate.isolateserver.ISOLATED_FILE_VERSION,
    }
    self._cleanup_isolated(expected_isolated)
    self.assertEqual(expected_isolated, actual_isolated)

    expected_saved_state = {
      'algo': 'sha-1',
      'child_isolated_files': [],
      'command': [],
      'config_variables': {
        'OS': isolate.get_flavor(),
        'chromeos': chromeos_value,
      },
      'extra_variables': {
        'foo': 'bar',
      },
      'files': {
        os.path.join(u'tests', 'isolate', 'files1', 'subdir', '42.txt'): {
          'm': 416,
          'h': hash_file('tests', 'isolate', 'files1', 'subdir', '42.txt'),
          's': _size('tests', 'isolate', 'files1', 'subdir', '42.txt'),
        },
        os.path.join(u'tests', 'isolate', 'files1', 'test_file1.txt'): {
          'm': 416,
          'h': hash_file('tests', 'isolate', 'files1', 'test_file1.txt'),
          's': _size('tests', 'isolate', 'files1', 'test_file1.txt'),
        },
        os.path.join(u'tests', 'isolate', 'files1', 'test_file2.txt'): {
          'm': 416,
          'h': hash_file('tests', 'isolate', 'files1', 'test_file2.txt'),
          's': _size('tests', 'isolate', 'files1', 'test_file2.txt'),
        },
        os.path.join(u'tests', 'isolate', 'no_run.isolate'): {
          'm': 416,
          'h': hash_file('tests', 'isolate', 'no_run.isolate'),
          's': _size('tests', 'isolate', 'no_run.isolate'),
        },
      },
      'isolate_file': file_path.safe_relpath(
          file_path.get_native_path_case(isolate_file),
          os.path.dirname(options.isolated)),
      'relative_cwd': os.path.join(u'tests', 'isolate'),
      'path_variables': {
        'PRODUCT_DIR': os.path.join(u'..', '..', 'third_party'),
      },
      'version': isolate.isolateserver.ISOLATED_FILE_VERSION,
    }
    self._cleanup_isolated(expected_saved_state)
    self._cleanup_saved_state(actual_saved_state)
    self.assertEqual(expected_saved_state, actual_saved_state)
    self.assertEqual([], os.listdir(self.directory))

  def test_chromium_split(self):
    # Create an .isolate file and a tree of random stuff.
    isolate_file = os.path.join(
        ROOT_DIR, 'tests', 'isolate', 'split.isolate')
    options = self._get_option(isolate_file)
    options.path_variables = {
      'DEPTH': '.',
      'PRODUCT_DIR': os.path.join('files1'),
    }
    options.config_variables = {
      'OS': isolate.get_flavor(),
    }
    complete_state = isolate.load_complete_state(
        options, os.path.join(ROOT_DIR, 'tests', 'isolate'), None, False)
    # By saving the files, it forces splitting the data up.
    complete_state.save_files()

    actual_isolated_master = tools.read_json(
        os.path.join(self.directory, 'foo.isolated'))
    expected_isolated_master = {
      u'algo': u'sha-1',
      u'command': [u'python', u'split.py'],
      u'files': {
        u'split.py': {
          u'm': 488,
          u'h': unicode(hash_file('tests', 'isolate', 'split.py')),
          u's': _size('tests', 'isolate', 'split.py'),
        },
      },
      u'includes': [
        unicode(hash_file(os.path.join(self.directory, 'foo.0.isolated'))),
        unicode(hash_file(os.path.join(self.directory, 'foo.1.isolated'))),
      ],
      u'os': unicode(isolate.get_flavor()),
      u'relative_cwd': u'.',
      u'version': unicode(isolate.isolateserver.ISOLATED_FILE_VERSION),
    }
    self._cleanup_isolated(expected_isolated_master)
    self.assertEqual(expected_isolated_master, actual_isolated_master)

    actual_isolated_0 = tools.read_json(
        os.path.join(self.directory, 'foo.0.isolated'))
    expected_isolated_0 = {
      u'algo': u'sha-1',
      u'files': {
        os.path.join(u'test', 'data', 'foo.txt'): {
          u'm': 416,
          u'h': unicode(
              hash_file('tests', 'isolate', 'test', 'data', 'foo.txt')),
          u's': _size('tests', 'isolate', 'test', 'data', 'foo.txt'),
        },
      },
      u'os': unicode(isolate.get_flavor()),
      u'version': unicode(isolate.isolateserver.ISOLATED_FILE_VERSION),
    }
    self._cleanup_isolated(expected_isolated_0)
    self.assertEqual(expected_isolated_0, actual_isolated_0)

    actual_isolated_1 = tools.read_json(
        os.path.join(self.directory, 'foo.1.isolated'))
    expected_isolated_1 = {
      u'algo': u'sha-1',
      u'files': {
        os.path.join(u'files1', 'subdir', '42.txt'): {
          u'm': 416,
          u'h': unicode(
              hash_file('tests', 'isolate', 'files1', 'subdir', '42.txt')),
          u's': _size('tests', 'isolate', 'files1', 'subdir', '42.txt'),
        },
      },
      u'os': unicode(isolate.get_flavor()),
      u'version': unicode(isolate.isolateserver.ISOLATED_FILE_VERSION),
    }
    self._cleanup_isolated(expected_isolated_1)
    self.assertEqual(expected_isolated_1, actual_isolated_1)

    actual_saved_state = tools.read_json(
        isolate.isolatedfile_to_state(options.isolated))
    isolated_base = unicode(os.path.basename(options.isolated))
    expected_saved_state = {
      u'algo': u'sha-1',
      u'child_isolated_files': [
        isolated_base[:-len('.isolated')] + '.0.isolated',
        isolated_base[:-len('.isolated')] + '.1.isolated',
      ],
      u'command': [u'python', u'split.py'],
      u'config_variables': {
        u'OS': unicode(isolate.get_flavor()),
      },
      u'extra_variables': {
        u'foo': u'bar',
      },
      u'files': {
        os.path.join(u'files1', 'subdir', '42.txt'): {
          u'm': 416,
          u'h': unicode(
              hash_file('tests', 'isolate', 'files1', 'subdir', '42.txt')),
          u's': _size('tests', 'isolate', 'files1', 'subdir', '42.txt'),
        },
        u'split.py': {
          u'm': 488,
          u'h': unicode(hash_file('tests', 'isolate', 'split.py')),
          u's': _size('tests', 'isolate', 'split.py'),
        },
        os.path.join(u'test', 'data', 'foo.txt'): {
          u'm': 416,
          u'h': unicode(
              hash_file('tests', 'isolate', 'test', 'data', 'foo.txt')),
          u's': _size('tests', 'isolate', 'test', 'data', 'foo.txt'),
        },
      },
      u'isolate_file': file_path.safe_relpath(
          file_path.get_native_path_case(isolate_file),
          unicode(os.path.dirname(options.isolated))),
      u'relative_cwd': u'.',
      u'path_variables': {
        u'DEPTH': u'.',
        u'PRODUCT_DIR': u'files1',
      },
      u'version': unicode(isolate.isolateserver.ISOLATED_FILE_VERSION),
    }
    self._cleanup_isolated(expected_saved_state)
    self._cleanup_saved_state(actual_saved_state)
    self.assertEqual(expected_saved_state, actual_saved_state)
    self.assertEqual(
        [
          'foo.0.isolated', 'foo.1.isolated',
          'foo.isolated', 'foo.isolated.state',
        ],
        sorted(os.listdir(self.directory)))


class IsolateCommand(IsolateBase):
  def load_complete_state(self, *_):
    """Creates a minimalist CompleteState instance without an .isolated
    reference.
    """
    out = isolate.CompleteState(None, isolate.SavedState(self.cwd))
    out.saved_state.isolate_file = u'blah.isolate'
    out.saved_state.relative_cwd = u''
    return out

  def test_CMDarchive(self):
    actual = []
    self.mock(
        isolate.isolateserver, 'upload_tree',
        lambda **kwargs: actual.append(kwargs))

    isolate_file = os.path.join(self.cwd, 'x.isolate')
    isolated_file = os.path.join(self.cwd, 'x.isolated')
    with open(isolate_file, 'wb') as f:
      f.write(
          '# Foo\n'
          '{'
          '  \'conditions\':['
          '    [\'OS=="dendy"\', {'
          '      \'variables\': {'
          '        \'isolate_dependency_tracked\': [\'foo\'],'
          '      },'
          '    }],'
          '  ],'
          '}')
    with open(os.path.join(self.cwd, 'foo'), 'wb') as f:
      f.write('fooo')

    self.mock(sys, 'stdout', cStringIO.StringIO())
    cmd = [
        '-i', isolate_file,
        '-s', isolated_file,
        '--isolate-server', 'http://localhost:1',
        '--config-variable', 'OS', 'dendy',
    ]
    self.assertEqual(0, isolate.CMDarchive(isolate.OptionParserIsolate(), cmd))
    expected = [
        {
          'base_url': 'http://localhost:1',
          'indir': self.cwd,
          'infiles': {
            isolated_file: {
              'priority': '0',
            },
            u'foo': {
              'h': '520d41b29f891bbaccf31d9fcfa72e82ea20fcf0',
              's': 4,
            },
          },
          'namespace': 'default-gzip',
        },
    ]
    # These always change.
    actual[0]['infiles'][isolated_file].pop('h')
    actual[0]['infiles'][isolated_file].pop('s')
    actual[0]['infiles']['foo'].pop('m')
    actual[0]['infiles']['foo'].pop('t')
    self.assertEqual(expected, actual)

  def test_CMDcheck_empty(self):
    isolate_file = os.path.join(self.cwd, 'x.isolate')
    isolated_file = os.path.join(self.cwd, 'x.isolated')
    with open(isolate_file, 'wb') as f:
      f.write('# Foo\n{\n}')

    self.mock(sys, 'stdout', cStringIO.StringIO())
    cmd = ['-i', isolate_file, '-s', isolated_file]
    with self.assertRaises(isolate.isolateserver.ConfigError):
      isolate.CMDcheck(isolate.OptionParserIsolate(), cmd)

  def test_CMDcheck_stale_version(self):
    isolate_file = os.path.join(self.cwd, 'x.isolate')
    isolated_file = os.path.join(self.cwd, 'x.isolated')
    with open(isolate_file, 'wb') as f:
      f.write(
          '# Foo\n'
          '{'
          '  \'conditions\':['
          '    [\'OS=="dendy"\', {'
          '      \'variables\': {'
          '        \'command\': [\'foo\'],'
          '      },'
          '    }],'
          '  ],'
          '}')

    self.mock(sys, 'stdout', cStringIO.StringIO())
    cmd = [
        '-i', isolate_file,
        '-s', isolated_file,
        '--config-variable', 'OS=dendy',
    ]
    self.assertEqual(0, isolate.CMDcheck(isolate.OptionParserIsolate(), cmd))

    with open(isolate_file, 'rb') as f:
      actual = f.read()
    expected = (
        '# Foo\n{  \'conditions\':[    [\'OS=="dendy"\', {      '
        '\'variables\': {        \'command\': [\'foo\'],      },    }],  ],}')
    self.assertEqual(expected, actual)

    with open(isolated_file, 'rb') as f:
      actual_isolated = f.read()
    expected_isolated = (
        '{"algo":"sha-1","command":["foo"],"files":{},"os":"dendy",'
        '"relative_cwd":".","version":"%s"}'
    ) % isolate.isolateserver.ISOLATED_FILE_VERSION
    self.assertEqual(expected_isolated, actual_isolated)
    isolated_data = json.loads(actual_isolated)

    with open(isolated_file + '.state', 'rb') as f:
      actual_isolated_state = f.read()
    expected_isolated_state = (
        '{"algo":"sha-1","child_isolated_files":[],"command":["foo"],'
        '"config_variables":{"OS":"dendy"},'
        '"extra_variables":{"EXECUTABLE_SUFFIX":""},"files":{},'
        '"isolate_file":"x.isolate","path_variables":{},'
        '"relative_cwd":".","version":"%s"}'
    ) % isolate.isolateserver.ISOLATED_FILE_VERSION
    self.assertEqual(expected_isolated_state, actual_isolated_state)
    isolated_state_data = json.loads(actual_isolated_state)

    # Now edit the .isolated.state file to break the version number and make
    # sure it doesn't crash.
    with open(isolated_file + '.state', 'wb') as f:
      isolated_state_data['version'] = '100.42'
      json.dump(isolated_state_data, f)
    self.assertEqual(0, isolate.CMDcheck(isolate.OptionParserIsolate(), cmd))

    # Now edit the .isolated file to break the version number and make
    # sure it doesn't crash.
    with open(isolated_file, 'wb') as f:
      isolated_data['version'] = '100.42'
      json.dump(isolated_data, f)
    self.assertEqual(0, isolate.CMDcheck(isolate.OptionParserIsolate(), cmd))

    # Make sure the files were regenerated.
    with open(isolated_file, 'rb') as f:
      actual_isolated = f.read()
    self.assertEqual(expected_isolated, actual_isolated)
    with open(isolated_file + '.state', 'rb') as f:
      actual_isolated_state = f.read()
    self.assertEqual(expected_isolated_state, actual_isolated_state)

  def test_CMDcheck_new_variables(self):
    # Test bug #61.
    isolate_file = os.path.join(self.cwd, 'x.isolate')
    isolated_file = os.path.join(self.cwd, 'x.isolated')
    cmd = [
        '-i', isolate_file,
        '-s', isolated_file,
        '--config-variable', 'OS=dendy',
    ]
    with open(isolate_file, 'wb') as f:
      f.write(
          '# Foo\n'
          '{'
          '  \'conditions\':['
          '    [\'OS=="dendy"\', {'
          '      \'variables\': {'
          '        \'command\': [\'foo\'],'
          '        \'isolate_dependency_tracked\': [\'foo\'],'
          '      },'
          '    }],'
          '  ],'
          '}')
    with open(os.path.join(self.cwd, 'foo'), 'wb') as f:
      f.write('yeah')

    self.mock(sys, 'stdout', cStringIO.StringIO())
    self.assertEqual(0, isolate.CMDcheck(isolate.OptionParserIsolate(), cmd))

    # Now add a new config variable.
    with open(isolate_file, 'wb') as f:
      f.write(
          '# Foo\n'
          '{'
          '  \'conditions\':['
          '    [\'OS=="dendy"\', {'
          '      \'variables\': {'
          '        \'command\': [\'foo\'],'
          '        \'isolate_dependency_tracked\': [\'foo\'],'
          '      },'
          '    }],'
          '    [\'foo=="baz"\', {'
          '      \'variables\': {'
          '        \'isolate_dependency_tracked\': [\'bar\'],'
          '      },'
          '    }],'
          '  ],'
          '}')
    with open(os.path.join(self.cwd, 'bar'), 'wb') as f:
      f.write('yeah right!')

    # The configuration is OS=dendy and foo=bar. So it should load both
    # configurations.
    self.assertEqual(
        0,
        isolate.CMDcheck(
            isolate.OptionParserIsolate(),
            cmd + ['--config-variable', 'foo=bar']))

  def test_CMDrewrite(self):
    isolate_file = os.path.join(self.cwd, 'x.isolate')
    data = (
      '# Foo',
      '{',
      '}',
    )
    with open(isolate_file, 'wb') as f:
      f.write('\n'.join(data))

    self.mock(sys, 'stdout', cStringIO.StringIO())
    cmd = ['-i', isolate_file]
    self.assertEqual(0, isolate.CMDrewrite(isolate.OptionParserIsolate(), cmd))
    with open(isolate_file, 'rb') as f:
      actual = f.read()

    expected = "# Foo\n{\n  'conditions': [\n  ],\n}\n"
    self.assertEqual(expected, actual)

  if sys.platform != 'win32':
    def test_CMDcheck_no_mode_on_windows(self):
      # Store for Windows, make sure file mode are not included. Hopefully, run
      # this test on another OS.
      isolate_file = os.path.join(
          ROOT_DIR, 'tests', 'isolate', 'symlink_full.isolate')
      isolated_file = os.path.join(self.cwd, 'foo.isolated')
      cmd = [
        '-i', isolate_file,
        '--config-variable', 'OS', 'win',
        '--config-variable', 'chromeos', '0',
        '-s', isolated_file,
      ]
      self.assertEqual(0, isolate.CMDcheck(isolate.OptionParserIsolate(), cmd))
      with open(isolated_file, 'rb') as f:
        actual = json.load(f)
      mapped = [
        os.path.join(u'files2', 'subdir', '42.txt'),
        os.path.join(u'files2', 'test_file1.txt'),
        os.path.join(u'files2', 'test_file2.txt'),
        os.path.join(u'symlink_full.py'),
      ]
      files = dict(
          (
            f,
            {
              u'h': unicode(hash_file('tests', 'isolate', f)),
              u's': _size('tests', 'isolate', f),
            }
          )
          for f in mapped)
      expected = {
        u'algo': u'sha-1',
        u'command': [u'python', u'symlink_full.py'],
        u'files': files,
        u'os': u'win',
        u'relative_cwd': u'.',
        u'version': unicode(isolate.isolateserver.ISOLATED_FILE_VERSION),
      }
      self.assertEqual(expected, actual)

  def test_CMDrun_extra_args(self):
    cmd = [
      'run',
      '--isolate', 'blah.isolate',
      '--', 'extra_args',
    ]
    self.mock(isolate, 'load_complete_state', self.load_complete_state)
    self.mock(isolate.subprocess, 'call', lambda *_, **_kwargs: 0)
    self.assertEqual(0, isolate.CMDrun(isolate.OptionParserIsolate(), cmd))


def clear_env_vars():
  for e in ('ISOLATE_DEBUG', 'ISOLATE_SERVER'):
    os.environ.pop(e, None)


if __name__ == '__main__':
  logging.basicConfig(
      level=logging.DEBUG if '-v' in sys.argv else logging.ERROR,
      format='%(levelname)5s %(filename)15s(%(lineno)3d): %(message)s')
  if '-v' in sys.argv:
    unittest.TestCase.maxDiff = None
  clear_env_vars()
  unittest.main()
