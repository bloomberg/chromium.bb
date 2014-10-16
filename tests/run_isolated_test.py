#!/usr/bin/env python
# Copyright 2013 The Swarming Authors. All rights reserved.
# Use of this source code is governed under the Apache License, Version 2.0 that
# can be found in the LICENSE file.

# pylint: disable=R0201

import StringIO
import functools
import hashlib
import json
import logging
import os
import shutil
import subprocess
import sys
import tempfile
import unittest

ROOT_DIR = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, ROOT_DIR)
sys.path.insert(0, os.path.join(ROOT_DIR, 'third_party'))

import isolated_format
import isolateserver
import run_isolated
from depot_tools import auto_stub
from utils import file_path
from utils import tools

ALGO = hashlib.sha1


def write_content(filepath, content):
  with open(filepath, 'wb') as f:
    f.write(content)


def json_dumps(data):
  return json.dumps(data, sort_keys=True, separators=(',', ':'))


class StorageFake(object):
  def __init__(self, files):
    self._files = files.copy()

  def __enter__(self, *_):
    return self

  def __exit__(self, *_):
    pass

  @property
  def hash_algo(self):
    return ALGO

  def async_fetch(self, channel, _priority, digest, _size, sink):
    sink([self._files[digest]])
    channel.send_result(digest)


class RunIsolatedTest(auto_stub.TestCase):
  def setUp(self):
    super(RunIsolatedTest, self).setUp()
    self.tempdir = tempfile.mkdtemp(prefix='run_isolated_test')
    logging.debug(self.tempdir)
    self.mock(run_isolated, 'make_temp_dir', self.fake_make_temp_dir)
    self.mock(run_isolated.auth, 'ensure_logged_in', lambda _: None)

  def tearDown(self):
    for dirpath, dirnames, filenames in os.walk(self.tempdir, topdown=True):
      for filename in filenames:
        file_path.set_read_only(os.path.join(dirpath, filename), False)
      for dirname in dirnames:
        file_path.set_read_only(os.path.join(dirpath, dirname), False)
    shutil.rmtree(self.tempdir)
    super(RunIsolatedTest, self).tearDown()

  @property
  def run_test_temp_dir(self):
    """Where to map all files in run_isolated.run_tha_test."""
    return os.path.join(self.tempdir, 'run_tha_test')

  def fake_make_temp_dir(self, prefix, _root_dir=None):
    """Predictably returns directory for run_tha_test (one per test case)."""
    self.assertIn(prefix, ('run_tha_test', 'isolated_out'))
    temp_dir = os.path.join(self.tempdir, prefix)
    self.assertFalse(os.path.isdir(temp_dir))
    os.makedirs(temp_dir)
    return temp_dir

  def temp_join(self, *args):
    """Shortcut for joining path with self.run_test_temp_dir."""
    return os.path.join(self.run_test_temp_dir, *args)

  def test_main(self):
    self.mock(tools, 'disable_buffering', lambda: None)
    calls = []
    # Unused argument ''
    # pylint: disable=W0613
    def call(command, cwd, env):
      calls.append(command)
      return 0
    self.mock(subprocess, 'call', call)
    isolated = json_dumps(
        {
          'command': ['foo.exe', 'cmd with space'],
        })
    isolated_hash = ALGO(isolated).hexdigest()
    def get_storage(_isolate_server, _namespace):
      return StorageFake({isolated_hash:isolated})
    self.mock(isolateserver, 'get_storage', get_storage)

    cmd = [
        '--no-log',
        '--hash', isolated_hash,
        '--cache', self.tempdir,
        '--isolate-server', 'https://localhost',
    ]
    ret = run_isolated.main(cmd)
    self.assertEqual(0, ret)
    self.assertEqual([[self.temp_join(u'foo.exe'), u'cmd with space']], calls)

  def test_main_args(self):
    self.mock(tools, 'disable_buffering', lambda: None)
    calls = []
    # Unused argument ''
    # pylint: disable=W0613
    def call(command, cwd, env):
      calls.append(command)
      return 0
    self.mock(subprocess, 'call', call)
    isolated = json_dumps(
        {
          'command': ['foo.exe', 'cmd with space'],
        })
    isolated_hash = ALGO(isolated).hexdigest()
    def get_storage(_isolate_server, _namespace):
      return StorageFake({isolated_hash:isolated})
    self.mock(isolateserver, 'get_storage', get_storage)

    cmd = [
        '--no-log',
        '--hash', isolated_hash,
        '--cache', self.tempdir,
        '--isolate-server', 'https://localhost',
        '--',
        '--extraargs',
        'bar',
    ]
    ret = run_isolated.main(cmd)
    self.assertEqual(0, ret)
    self.assertEqual(
        [[self.temp_join(u'foo.exe'), u'cmd with space', '--extraargs', 'bar']],
        calls)

  def _run_tha_test(self, isolated_hash, files):
    make_tree_call = []
    def add(i, _):
      make_tree_call.append(i)
    for i in ('make_tree_read_only', 'make_tree_files_read_only',
              'make_tree_deleteable', 'make_tree_writeable'):
      self.mock(file_path, i, functools.partial(add, i))

    # Keeps tuple of (args, kwargs).
    subprocess_call = []
    self.mock(
        subprocess, 'call', lambda *x, **y: subprocess_call.append((x, y)) or 0)

    ret = run_isolated.run_tha_test(
        isolated_hash,
        StorageFake(files),
        isolateserver.MemoryCache(),
        False,
        [])
    self.assertEqual(0, ret)
    return subprocess_call, make_tree_call

  def test_run_tha_test_naked(self):
    isolated = json_dumps({'command': ['invalid', 'command']})
    isolated_hash = ALGO(isolated).hexdigest()
    files = {isolated_hash:isolated}
    subprocess_call, make_tree_call = self._run_tha_test(isolated_hash, files)
    self.assertEqual(
        ['make_tree_writeable', 'make_tree_deleteable', 'make_tree_deleteable'],
        make_tree_call)
    self.assertEqual(1, len(subprocess_call))
    self.assertTrue(subprocess_call[0][1].pop('cwd'))
    self.assertTrue(subprocess_call[0][1].pop('env'))
    self.assertEqual(
        [(([self.temp_join(u'invalid'), u'command'],), {})], subprocess_call)

  def test_run_tha_test_naked_read_only_0(self):
    isolated = json_dumps(
        {
          'command': ['invalid', 'command'],
          'read_only': 0,
        })
    isolated_hash = ALGO(isolated).hexdigest()
    files = {isolated_hash:isolated}
    subprocess_call, make_tree_call = self._run_tha_test(isolated_hash, files)
    self.assertEqual(
        ['make_tree_writeable', 'make_tree_deleteable', 'make_tree_deleteable'],
        make_tree_call)
    self.assertEqual(1, len(subprocess_call))
    self.assertTrue(subprocess_call[0][1].pop('cwd'))
    self.assertTrue(subprocess_call[0][1].pop('env'))
    self.assertEqual(
        [(([self.temp_join(u'invalid'), u'command'],), {})], subprocess_call)

  def test_run_tha_test_naked_read_only_1(self):
    isolated = json_dumps(
        {
          'command': ['invalid', 'command'],
          'read_only': 1,
        })
    isolated_hash = ALGO(isolated).hexdigest()
    files = {isolated_hash:isolated}
    subprocess_call, make_tree_call = self._run_tha_test(isolated_hash, files)
    self.assertEqual(
        [
          'make_tree_files_read_only', 'make_tree_deleteable',
          'make_tree_deleteable',
        ],
        make_tree_call)
    self.assertEqual(1, len(subprocess_call))
    self.assertTrue(subprocess_call[0][1].pop('cwd'))
    self.assertTrue(subprocess_call[0][1].pop('env'))
    self.assertEqual(
        [(([self.temp_join(u'invalid'), u'command'],), {})], subprocess_call)

  def test_run_tha_test_naked_read_only_2(self):
    isolated = json_dumps(
        {
          'command': ['invalid', 'command'],
          'read_only': 2,
        })
    isolated_hash = ALGO(isolated).hexdigest()
    files = {isolated_hash:isolated}
    subprocess_call, make_tree_call = self._run_tha_test(isolated_hash, files)
    self.assertEqual(
        ['make_tree_read_only', 'make_tree_deleteable', 'make_tree_deleteable'],
        make_tree_call)
    self.assertEqual(1, len(subprocess_call))
    self.assertTrue(subprocess_call[0][1].pop('cwd'))
    self.assertTrue(subprocess_call[0][1].pop('env'))
    self.assertEqual(
        [(([self.temp_join(u'invalid'), u'command'],), {})], subprocess_call)

  def test_main_naked(self):
    # The most naked .isolated file that can exist.
    self.mock(tools, 'disable_buffering', lambda: None)
    isolated = json_dumps({'command': ['invalid', 'command']})
    isolated_hash = ALGO(isolated).hexdigest()
    def get_storage(_isolate_server, _namespace):
      return StorageFake({isolated_hash:isolated})
    self.mock(isolateserver, 'get_storage', get_storage)

    # Keeps tuple of (args, kwargs).
    subprocess_call = []
    self.mock(
        subprocess, 'call', lambda *x, **y: subprocess_call.append((x, y)) or 8)

    cmd = [
        '--no-log',
        '--hash', isolated_hash,
        '--cache', self.tempdir,
        '--isolate-server', 'https://localhost',
    ]
    ret = run_isolated.main(cmd)
    self.assertEqual(8, ret)
    self.assertEqual(1, len(subprocess_call))
    self.assertTrue(subprocess_call[0][1].pop('cwd'))
    self.assertTrue(subprocess_call[0][1].pop('env'))
    self.assertEqual(
        [(([self.temp_join(u'invalid'), u'command'],), {})], subprocess_call)

  def test_modified_cwd(self):
    isolated = json_dumps({
        'command': ['../out/some.exe', 'arg'],
        'relative_cwd': 'some',
    })
    isolated_hash = ALGO(isolated).hexdigest()
    files = {isolated_hash:isolated}
    subprocess_call, _ = self._run_tha_test(isolated_hash, files)
    self.assertEqual(1, len(subprocess_call))
    self.assertEqual(subprocess_call[0][1].pop('cwd'), self.temp_join('some'))
    self.assertTrue(subprocess_call[0][1].pop('env'))
    self.assertEqual(
        [(([self.temp_join(u'out', u'some.exe'), 'arg'],), {})],
        subprocess_call)

  def test_python_cmd(self):
    isolated = json_dumps({
        'command': ['../out/cmd.py', 'arg'],
        'relative_cwd': 'some',
    })
    isolated_hash = ALGO(isolated).hexdigest()
    files = {isolated_hash:isolated}
    subprocess_call, _ = self._run_tha_test(isolated_hash, files)
    self.assertEqual(1, len(subprocess_call))
    self.assertEqual(subprocess_call[0][1].pop('cwd'), self.temp_join('some'))
    self.assertTrue(subprocess_call[0][1].pop('env'))
    # Injects sys.executable.
    self.assertEqual(
        [(([sys.executable, os.path.join('..', 'out', 'cmd.py'), 'arg'],), {})],
        subprocess_call)

  def test_output(self):
    script = (
      'import sys\n'
      'open(sys.argv[1], "w").write("bar")\n')
    script_hash = ALGO(script).hexdigest()
    isolated = json_dumps(
        {
          'algo': 'sha-1',
          'command': ['cmd.py', '${ISOLATED_OUTDIR}/foo'],
          'files': {
            'cmd.py': {
              'h': script_hash,
              'm': 0700,
              's': len(script),
            },
          },
          'version': isolated_format.ISOLATED_FILE_VERSION,
        })
    isolated_hash = ALGO(isolated).hexdigest()
    contents = {
        isolated_hash: isolated,
        script_hash: script,
    }

    path = os.path.join(self.tempdir, 'store')
    os.mkdir(path)
    for h, c in contents.iteritems():
      write_content(os.path.join(path, h), c)
    store = isolateserver.get_storage(path, 'default-store')

    self.mock(sys, 'stdout', StringIO.StringIO())
    ret = run_isolated.run_tha_test(
        isolated_hash,
        store,
        isolateserver.MemoryCache(),
        False,
        [])
    self.assertEqual(0, ret)

    # It uploaded back. Assert the store has a new item containing foo.
    hashes = set(contents)
    output_hash = ALGO('bar').hexdigest()
    hashes.add(output_hash)
    uploaded = json_dumps(
        {
          'algo': 'sha-1',
          'files': {
            'foo': {
              'h': output_hash,
              # TODO(maruel): Handle umask.
              'm': 0640,
              's': 3,
            },
          },
          'version': isolated_format.ISOLATED_FILE_VERSION,
        })
    uploaded_hash = ALGO(uploaded).hexdigest()
    hashes.add(uploaded_hash)
    self.assertEqual(hashes, set(os.listdir(path)))

    expected = ''.join([
      '[run_isolated_out_hack]',
      '{"hash":"%s","namespace":"default-store","storage":"%s"}' % (
          uploaded_hash, path),
      '[/run_isolated_out_hack]'
    ]) + '\n'
    self.assertEqual(expected, sys.stdout.getvalue())


if __name__ == '__main__':
  logging.basicConfig(
      level=logging.DEBUG if '-v' in sys.argv else logging.ERROR)
  unittest.main()
