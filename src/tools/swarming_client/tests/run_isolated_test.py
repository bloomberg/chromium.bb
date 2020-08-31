#!/usr/bin/env vpython
# Copyright 2013 The LUCI Authors. All rights reserved.
# Use of this source code is governed under the Apache License, Version 2.0
# that can be found in the LICENSE file.

import StringIO
import base64
import contextlib
import functools
import hashlib
import json
import logging
import os
import sys
import tempfile

import six

# Mutates sys.path.
import test_env

# third_party/
from depot_tools import auto_stub

import isolateserver_fake
import cipdserver_fake

import cipd
import isolate_storage
import isolated_format
import isolateserver
import local_caching
import run_isolated
from libs import luci_context
from utils import file_path
from utils import fs
from utils import large
from utils import logging_utils
from utils import on_error
from utils import subprocess42
from utils import tools


ALGO = hashlib.sha1


def write_content(filepath, content):
  with open(filepath, 'wb') as f:
    f.write(content)


def json_dumps(data):
  return json.dumps(data, sort_keys=True, separators=(',', ':'))


def read_tree(path):
  """Returns a dict with {filepath: content}."""
  if not fs.isdir(path):
    return None
  out = {}
  for root, _, filenames in fs.walk(path):
    for filename in filenames:
      p = os.path.join(root, filename)
      with fs.open(p, 'rb') as f:
        out[os.path.relpath(p, path)] = f.read()
  return out


@contextlib.contextmanager
def init_named_caches_stub(_run_dir):
  yield


def put_to_named_cache(manager, cache_name, file_name, contents):
  """Puts files into named cache."""
  tdir = tempfile.mkdtemp(prefix=u'run_isolated_test')
  try:
    cache_dir = os.path.join(tdir, 'cache')
    manager.install(cache_dir, cache_name)
    with open(os.path.join(cache_dir, file_name), 'wb') as f:
      f.write(contents)
    manager.uninstall(cache_dir, cache_name)
  finally:
    file_path.rmtree(tdir)


class StorageFake(object):
  def __init__(self, files, server_ref):
    self._files = files.copy()
    self._server_ref = server_ref

  def __enter__(self, *_):
    return self

  def __exit__(self, *_):
    pass

  @property
  def server_ref(self):
    return self._server_ref

  def async_fetch(self, channel, _priority, digest, _size, sink):
    sink([self._files[digest]])
    channel.send_result(digest)

  def upload_items(self, items_to_upload, _verify_push):
    # Return all except the first one.
    return list(items_to_upload)[1:]


class RunIsolatedTestBase(auto_stub.TestCase):
  # These tests fail with the following error
  # 'AssertionError: Items in the first set but not the second'
  # Need to run in test_seq.py as an executable
  no_run = 1

  @classmethod
  def setUpClass(cls):
    if not file_path.enable_symlink():
      raise Exception(
          'Failed to enable symlink; this test requires it. On Windows, maybe '
          'try running as Administrator')

  def setUp(self):
    super(RunIsolatedTestBase, self).setUp()
    os.environ.pop('LUCI_CONTEXT', None)
    self._previous_dir = six.text_type(os.getcwd())
    self.tempdir = tempfile.mkdtemp(prefix=u'run_isolated_test')
    logging.debug('Temp dir: %s', self.tempdir)
    cwd = os.path.join(self.tempdir, 'cwd')
    fs.mkdir(cwd)
    os.chdir(cwd)
    self.mock(run_isolated, 'make_temp_dir', self.fake_make_temp_dir)
    self.mock(run_isolated.auth, 'ensure_logged_in', lambda _: None)
    self.mock(
        logging_utils.OptionParserWithLogging, 'logger_root',
        logging.Logger('unittest'))

    self._cipd_server = None  # initialized lazily

  def tearDown(self):
    # Remove mocks.
    super(RunIsolatedTestBase, self).tearDown()
    fs.chdir(self._previous_dir)
    file_path.rmtree(self.tempdir)
    if self._cipd_server:
      self._cipd_server.close()

  @property
  def cipd_server(self):
    if not self._cipd_server:
      self._cipd_server = cipdserver_fake.FakeCipdServer()
    return self._cipd_server

  def fake_make_temp_dir(self, prefix, _root_dir):
    """Predictably returns directory for run_tha_test (one per test case)."""
    self.assertIn(prefix,
                  (run_isolated.ISOLATED_OUT_DIR, run_isolated.ISOLATED_RUN_DIR,
                   run_isolated.ISOLATED_TMP_DIR,
                   run_isolated.ISOLATED_CLIENT_DIR, 'cipd_site_root'))
    temp_dir = os.path.join(self.tempdir, prefix)
    self.assertFalse(fs.isdir(temp_dir))
    fs.makedirs(temp_dir)
    return temp_dir

  def ir_dir(self, *args):
    """Shortcut for joining path with ISOLATED_RUN_DIR.

    Where to map all files in run_isolated.run_tha_test().
    """
    return os.path.join(self.tempdir, run_isolated.ISOLATED_RUN_DIR, *args)


class RunIsolatedTest(RunIsolatedTestBase):
  # Mocked Popen so no subprocess is started.
  def setUp(self):
    super(RunIsolatedTest, self).setUp()
    # list of func(args, **kwargs) -> retcode
    # if the func returns None, then it's skipped. The first function to return
    # non-None is taken as the retcode for the mocked Popen call.
    self.popen_fakes = []
    self.popen_calls = []

    self.capture_popen_env = False
    self.capture_luci_ctx = False

    # pylint: disable=no-self-argument
    class Popen(object):
      def __init__(self2, args, **kwargs):
        if not self.capture_popen_env:
          kwargs.pop('env', None)
        if self.capture_luci_ctx:
          with open(os.environ['LUCI_CONTEXT']) as f:
            kwargs['luci_ctx'] = json.load(f)
        self2.returncode = None
        self2.args = args
        self2.kwargs = kwargs
        self.popen_calls.append((args, kwargs))

      def yield_any_line(self2, timeout=None):
        self.assertEqual(0.1, timeout)
        return ()

      def wait(self2, timeout=None):
        self.assertIn(timeout, (None, 30, 60))
        self2.returncode = 0
        for mock_fn in self.popen_fakes:
          ret = mock_fn(self2.args, **self2.kwargs)
          if ret is not None:
            self2.returncode = ret
            break
        return self2.returncode

      def kill(self):
        pass

    self.mock(subprocess42, 'Popen', Popen)

  def test_get_command_env(self):
    old_env = os.environ
    try:
      os.environ = os.environ.copy()
      os.environ.pop('B', None)
      self.assertNotIn('B', os.environ)
      os.environ['C'] = 'foo'
      os.environ['D'] = 'bar'
      os.environ['E'] = 'baz'
      env = run_isolated.get_command_env(
          '/a',
          None,
          '/b',
          {
              'A': 'a',
              'B': None,
              'C': None,
              'E': '${ISOLATED_OUTDIR}/eggs'
          },
          {'D': ['foo']},
          '/spam',
          None)
      self.assertNotIn('B', env)
      self.assertNotIn('C', env)
      if sys.platform == 'win32':
        self.assertEqual('\\b\\foo;bar', env['D'])
      else:
        self.assertEqual('/b/foo:bar', env['D'])
      self.assertEqual(os.sep + os.path.join('spam', 'eggs'), env['E'])
    finally:
      os.environ = old_env

  def test_main(self):
    self.mock(tools, 'disable_buffering', lambda: None)
    isolated = json_dumps(
        {
          'command': ['foo.exe', 'cmd with space'],
        })
    isolated_hash = isolateserver_fake.hash_content(isolated)
    def get_storage(server_ref):
      return StorageFake({isolated_hash: isolated}, server_ref)
    self.mock(isolateserver, 'get_storage', get_storage)

    cmd = [
        '--no-log',
        '--isolated',
        isolated_hash,
        '--cache',
        os.path.join(self.tempdir, 'isolated_cache'),
        '--named-cache-root',
        os.path.join(self.tempdir, 'named_cache'),
        '--isolate-server',
        'https://localhost',
        '--root-dir',
        self.tempdir,
    ]
    ret = run_isolated.main(cmd)
    self.assertEqual(0, ret)
    self.assertEqual(
        [
          (
            [self.ir_dir(u'foo.exe'), u'cmd with space'],
            {
              'cwd': self.ir_dir(),
              'detached': True,
              'close_fds': True,
              'lower_priority': False,
              'containment': subprocess42.Containment(),
            },
          ),
        ],
        self.popen_calls)

  def test_main_args(self):
    self.mock(tools, 'disable_buffering', lambda: None)
    isolated = json_dumps({'command': ['foo.exe', 'cmd w/ space']})
    isolated_hash = isolateserver_fake.hash_content(isolated)
    def get_storage(server_ref):
      return StorageFake({isolated_hash: isolated}, server_ref)
    self.mock(isolateserver, 'get_storage', get_storage)

    cmd = [
        '--no-log',
        '--isolated',
        isolated_hash,
        '--cache',
        os.path.join(self.tempdir, 'isolated_cache'),
        '--isolate-server',
        'https://localhost',
        '--named-cache-root',
        os.path.join(self.tempdir, 'named_cache'),
        '--root-dir',
        self.tempdir,
        '--',
        '--extraargs',
        'bar',
    ]
    ret = run_isolated.main(cmd)
    self.assertEqual(0, ret)
    self.assertEqual(
        [
          (
            [self.ir_dir(u'foo.exe'), u'cmd w/ space', '--extraargs', 'bar'],
            {
              'cwd': self.ir_dir(),
              'detached': True,
              'close_fds': True,
              'lower_priority': False,
              'containment': subprocess42.Containment(),
            },
          ),
        ],
        self.popen_calls)

  def _run_tha_test(
      self, isolated_hash=None, files=None, command=None,
      lower_priority=False):
    files = files or {}
    make_tree_call = []
    def add(i, _):
      make_tree_call.append(i)
    for i in ('make_tree_read_only', 'make_tree_files_read_only',
              'make_tree_deleteable', 'make_tree_writeable'):
      self.mock(file_path, i, functools.partial(add, i))

    server_ref = isolate_storage.ServerRef('http://localhost:1', 'default-gzip')
    data = run_isolated.TaskData(
        command=command or [],
        relative_cwd=None,
        extra_args=[],
        isolated_hash=isolated_hash,
        storage=StorageFake(files, server_ref),
        isolate_cache=local_caching.MemoryContentAddressedCache(),
        outputs=None,
        install_named_caches=init_named_caches_stub,
        leak_temp_dir=False,
        root_dir=None,
        hard_timeout=60,
        grace_period=30,
        bot_file=None,
        switch_to_account=False,
        install_packages_fn=run_isolated.noop_install_packages,
        use_go_isolated=False,
        go_cache_dir=None,
        go_cache_policies=None,
        env={},
        env_prefix={},
        lower_priority=lower_priority,
        containment=None)
    ret = run_isolated.run_tha_test(data, None)
    self.assertEqual(0, ret)
    return make_tree_call

  def test_run_tha_test_naked(self):
    isolated = json_dumps({'command': ['invalid', 'command']})
    isolated_hash = isolateserver_fake.hash_content(isolated)
    files = {isolated_hash: isolated}
    make_tree_call = self._run_tha_test(isolated_hash, files)
    self.assertEqual([
        'make_tree_writeable',
        'make_tree_deleteable',
        'make_tree_deleteable',
        'make_tree_deleteable',
        'make_tree_deleteable',
    ], make_tree_call)
    self.assertEqual(
        [
          (
            [self.ir_dir(u'invalid'), u'command'],
            {
              'cwd': self.ir_dir(),
              'detached': True,
              'close_fds': True,
              'lower_priority': False,
              'containment': None,
            },
          ),
        ],
        self.popen_calls)

  def test_run_tha_test_naked_read_only_0(self):
    isolated = json_dumps(
        {
          'command': ['invalid', 'command'],
          'read_only': 0,
        })
    isolated_hash = isolateserver_fake.hash_content(isolated)
    files = {isolated_hash: isolated}
    make_tree_call = self._run_tha_test(isolated_hash, files)
    self.assertEqual([
        'make_tree_writeable',
        'make_tree_deleteable',
        'make_tree_deleteable',
        'make_tree_deleteable',
        'make_tree_deleteable',
    ], make_tree_call)
    self.assertEqual(
        [
          (
            [self.ir_dir(u'invalid'), u'command'],
            {
              'cwd': self.ir_dir(),
              'detached': True,
              'close_fds': True,
              'lower_priority': False,
              'containment': None,
            },
          ),
        ],
        self.popen_calls)

  def test_run_tha_test_naked_read_only_1(self):
    isolated = json_dumps(
        {
          'command': ['invalid', 'command'],
          'read_only': 1,
        })
    isolated_hash = isolateserver_fake.hash_content(isolated)
    files = {isolated_hash: isolated}
    make_tree_call = self._run_tha_test(isolated_hash, files)
    self.assertEqual([
        'make_tree_files_read_only',
        'make_tree_deleteable',
        'make_tree_deleteable',
        'make_tree_deleteable',
        'make_tree_deleteable',
    ], make_tree_call)
    self.assertEqual(
        [
          (
            [self.ir_dir(u'invalid'), u'command'],
            {
              'cwd': self.ir_dir(),
              'detached': True,
              'close_fds': True,
              'lower_priority': False,
              'containment': None,
            },
          ),
        ],
        self.popen_calls)

  def test_run_tha_test_naked_read_only_2(self):
    isolated = json_dumps(
        {
          'command': ['invalid', 'command'],
          'read_only': 2,
        })
    isolated_hash = isolateserver_fake.hash_content(isolated)
    files = {isolated_hash: isolated}
    make_tree_call = self._run_tha_test(isolated_hash, files)
    self.assertEqual([
        'make_tree_read_only',
        'make_tree_deleteable',
        'make_tree_deleteable',
        'make_tree_deleteable',
        'make_tree_deleteable',
    ], make_tree_call)
    self.assertEqual(
        [
          (
            [self.ir_dir(u'invalid'), u'command'],
            {
              'cwd': self.ir_dir(),
              'detached': True,
              'close_fds': True,
              'lower_priority': False,
              'containment': None,
            },
          ),
        ],
        self.popen_calls)

  def mock_popen_with_oserr(self):
    def r(self, args, **kwargs):
      old_init(self, args, **kwargs)
      raise OSError('Unknown')
    old_init = self.mock(subprocess42.Popen, '__init__', r)

  def test_main_naked(self):
    self.mock_popen_with_oserr()
    self.mock(on_error, 'report', lambda _: None)
    # The most naked .isolated file that can exist.
    self.mock(tools, 'disable_buffering', lambda: None)
    isolated = json_dumps({'command': ['invalid', 'command']})
    isolated_hash = isolateserver_fake.hash_content(isolated)
    def get_storage(server_ref):
      return StorageFake({isolated_hash: isolated}, server_ref)
    self.mock(isolateserver, 'get_storage', get_storage)

    cmd = [
        '--no-log',
        '--isolated',
        isolated_hash,
        '--cache',
        os.path.join(self.tempdir, 'isolated_cache'),
        '--isolate-server',
        'https://localhost',
        '--named-cache-root',
        os.path.join(self.tempdir, 'named_cache'),
        '--root-dir',
        self.tempdir,
    ]
    ret = run_isolated.main(cmd)
    self.assertEqual(1, ret)
    self.assertEqual(1, len(self.popen_calls))
    self.assertEqual(
        [
          (
            [self.ir_dir(u'invalid'), u'command'],
            {
              'cwd': self.ir_dir(),
              'detached': True,
              'close_fds': True,
              'lower_priority': False,
              'containment': subprocess42.Containment(),
            },
          ),
        ],
        self.popen_calls)

  def test_main_naked_without_isolated(self):
    self.mock_popen_with_oserr()
    cmd = [
        '--no-log',
        '--cache',
        os.path.join(self.tempdir, 'isolated_cache'),
        '--named-cache-root',
        os.path.join(self.tempdir, 'named_cache'),
        '--raw-cmd',
        '--',
        '/bin/echo',
        'hello',
        'world',
    ]
    ret = run_isolated.main(cmd)
    self.assertEqual(1, ret)
    self.assertEqual(
        [
          (
            ['/bin/echo', 'hello', 'world'],
            {
              'cwd': self.ir_dir(),
              'detached': True,
              'close_fds': True,
              'lower_priority': False,
              'containment': subprocess42.Containment(),
            },
          ),
        ],
        self.popen_calls)

  def test_main_naked_with_account_switch(self):
    self.capture_luci_ctx = True
    self.mock_popen_with_oserr()
    cmd = [
        '--no-log',
        '--cache',
        os.path.join(self.tempdir, 'isolated_cache'),
        '--named-cache-root',
        os.path.join(self.tempdir, 'named_cache'),
        '--switch-to-account',
        'task',
        '--raw-cmd',
        '--',
        '/bin/echo',
        'hello',
        'world',
    ]
    root_ctx = {
      'accounts': [{'id': 'bot'}, {'id': 'task'}],
      'default_account_id' : 'bot',
      'secret': 'sekret',
      'rpc_port': 12345,
    }
    with luci_context.write(local_auth=root_ctx):
      run_isolated.main(cmd)
    # Switched default account to task.
    task_ctx = root_ctx.copy()
    task_ctx['default_account_id'] = 'task'
    self.assertEqual(task_ctx, self.popen_calls[0][1]['luci_ctx']['local_auth'])

  def test_main_naked_with_account_pop(self):
    self.capture_luci_ctx = True
    self.mock_popen_with_oserr()
    cmd = [
        '--no-log',
        '--cache',
        os.path.join(self.tempdir, 'isolated_cache'),
        '--named-cache-root',
        os.path.join(self.tempdir, 'named_cache'),
        '--switch-to-account',
        'task',
        '--raw-cmd',
        '--',
        '/bin/echo',
        'hello',
        'world',
    ]
    root_ctx = {
      'accounts': [{'id': 'bot'}],  # only 'bot', there's no 'task'
      'default_account_id' : 'bot',
      'secret': 'sekret',
      'rpc_port': 12345,
    }
    with luci_context.write(local_auth=root_ctx):
      run_isolated.main(cmd)
    # Unset default account, since 'task' account is not defined.
    task_ctx = root_ctx.copy()
    task_ctx.pop('default_account_id')
    self.assertEqual(task_ctx, self.popen_calls[0][1]['luci_ctx']['local_auth'])

  def test_main_naked_leaking(self):
    workdir = tempfile.mkdtemp()
    try:
      cmd = [
          '--no-log',
          '--cache',
          os.path.join(self.tempdir, 'isolated_cache'),
          '--root-dir',
          workdir,
          '--leak-temp-dir',
          '--named-cache-root',
          os.path.join(self.tempdir, 'named_cache'),
          '--raw-cmd',
          '--',
          '/bin/echo',
          'hello',
          'world',
      ]
      ret = run_isolated.main(cmd)
      self.assertEqual(0, ret)
    finally:
      fs.rmtree(unicode(workdir))

  def test_main_naked_with_packages(self):
    self.mock(cipd, 'get_platform', lambda: 'linux-amd64')

    def pins_generator():
      yield {
          '': [
              ('infra/data/x', 'badc0fee' * 5),
              ('infra/data/y', 'cafebabe' * 5),
          ],
          'bin': [('infra/tools/echo/linux-amd64', 'deadbeef' * 5),],
      }
      yield {
          '': [('infra/tools/luci/isolated/linux-amd64',
                run_isolated.ISOLATED_REVISION)],
      }

    pins_gen = pins_generator()

    suffix = '.exe' if sys.platform == 'win32' else ''
    def fake_ensure(args, **kwargs):
      if (args[0].endswith(os.path.join('bin', 'cipd' + suffix)) and
          args[1] == 'ensure'
          and '-json-output' in args):
        idx = args.index('-json-output')
        with open(args[idx+1], 'w') as json_out:
          json.dump({
              'result': {
                  subdir: [{
                      'package': pkg,
                      'instance_id': ver
                  } for pkg, ver in packages
                          ] for subdir, packages in pins_gen.next().items()
              }
          }, json_out)
        return 0
      if args[0].endswith(os.sep + 'echo' + suffix):
        return 0
      self.fail('unexpected: %s, %s' % (args, kwargs))
      return 1

    self.popen_fakes.append(fake_ensure)
    cipd_cache = os.path.join(self.tempdir, 'cipd_cache')
    cmd = [
        '--no-log',
        '--cache',
        os.path.join(self.tempdir, 'isolated_cache'),
        '--cipd-client-version',
        'git:wowza',
        '--cipd-package',
        'bin:infra/tools/echo/${platform}:latest',
        '--cipd-package',
        '.:infra/data/x:latest',
        '--cipd-package',
        '.:infra/data/y:canary',
        '--cipd-server',
        self.cipd_server.url,
        '--cipd-cache',
        cipd_cache,
        '--named-cache-root',
        os.path.join(self.tempdir, 'named_cache'),
        '--raw-cmd',
        '--',
        'bin/echo${EXECUTABLE_SUFFIX}',
        'hello',
        'world',
    ]
    ret = run_isolated.main(cmd)
    self.assertEqual(0, ret)

    self.assertEqual(3, len(self.popen_calls))

    # Test cipd-ensure command for installing packages.
    cipd_ensure_cmd, _ = self.popen_calls[0]
    self.assertEqual(cipd_ensure_cmd[:2], [
      os.path.join(cipd_cache, 'bin', 'cipd' + cipd.EXECUTABLE_SUFFIX),
      'ensure',
    ])
    cache_dir_index = cipd_ensure_cmd.index('-cache-dir')
    self.assertEqual(
        cipd_ensure_cmd[cache_dir_index+1],
        os.path.join(cipd_cache, 'cache'))

    # Test cipd client cache. `git:wowza` was a tag and so is cacheable.
    self.assertEqual(len(fs.listdir(os.path.join(cipd_cache, 'versions'))), 2)
    version_file = six.text_type(
        os.path.join(cipd_cache, 'versions',
                     '765a0de4c618f91faf923cb68a47bb564aed412d'))
    self.assertTrue(fs.isfile(version_file))
    with open(version_file) as f:
      self.assertEqual(f.read(), 'aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa')

    client_binary_file = six.text_type(
        os.path.join(cipd_cache, 'clients',
                     'aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa'))
    self.assertTrue(fs.isfile(client_binary_file))

    # Test echo call.
    echo_cmd, _ = self.popen_calls[2]
    self.assertTrue(echo_cmd[0].endswith(
        os.path.sep + 'bin' + os.path.sep + 'echo' + cipd.EXECUTABLE_SUFFIX),
        echo_cmd[0])
    self.assertEqual(echo_cmd[1:], [u'hello', u'world'])

  def test_main_naked_with_cipd_client_no_packages(self):
    self.mock(cipd, 'get_platform', lambda: 'linux-amd64')

    cipd_cache = os.path.join(self.tempdir, 'cipd_cache')
    cmd = [
        '--no-log',
        '--cache',
        os.path.join(self.tempdir, 'isolated_cache'),
        '--cipd-enabled',
        '--cipd-client-version',
        'git:wowza',
        '--cipd-server',
        self.cipd_server.url,
        '--cipd-cache',
        cipd_cache,
        '--named-cache-root',
        os.path.join(self.tempdir, 'named_cache'),
        '--raw-cmd',
        '--relative-cwd',
        'a',
        '--',
        'bin/echo${EXECUTABLE_SUFFIX}',
        'hello',
        'world',
    ]

    pins = {
        '': [('infra/tools/luci/isolated/linux-amd64',
              run_isolated.ISOLATED_REVISION)],
    }

    suffix = '.exe' if sys.platform == 'win32' else ''

    def fake_ensure(args, **kwargs):
      if (args[0].endswith(os.path.join('bin', 'cipd' + suffix)) and
          args[1] == 'ensure' and '-json-output' in args):
        idx = args.index('-json-output')
        with open(args[idx + 1], 'w') as json_out:
          json.dump({
              'result': {
                  subdir: [{
                      'package': pkg,
                      'instance_id': ver
                  } for pkg, ver in packages
                          ] for subdir, packages in pins.items()
              }
          }, json_out)
        return 0
      if args[0].endswith(os.sep + 'echo' + suffix):
        return 0
      self.fail('unexpected: %s, %s' % (args, kwargs))
      return 1

    self.popen_fakes.append(fake_ensure)

    self.capture_popen_env = True
    ret = run_isolated.main(cmd)
    self.assertEqual(0, ret)

    # The CIPD client was bootstrapped and hardlinked (or copied on Win).
    client_binary_file = six.text_type(
        os.path.join(cipd_cache, 'clients',
                     'aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa'))
    self.assertTrue(fs.isfile(client_binary_file))
    client_binary_link = six.text_type(
        os.path.join(cipd_cache, 'bin', 'cipd' + cipd.EXECUTABLE_SUFFIX))
    self.assertTrue(fs.isfile(client_binary_link))

    env = self.popen_calls[1][1].pop('env')
    exec_path = self.ir_dir(u'a', 'bin', 'echo')
    if sys.platform == 'win32':
      exec_path += '.exe'
    self.assertEqual(
        [
            (
                [exec_path, 'hello', 'world'],
                {
                    'cwd': self.ir_dir('a'),
                    'detached': True,
                    'close_fds': True,
                    'lower_priority': False,
                    'containment': subprocess42.Containment(),
                },
            ),
        ],
        # Ignore `cipd ensure` for isolated client here.
        self.popen_calls[1:])

    # Directory with cipd client is in front of PATH.
    path = env['PATH'].split(os.pathsep)
    self.assertEqual(os.path.join(cipd_cache, 'bin'), path[0])

    # CIPD_CACHE_DIR is set.
    self.assertEqual(os.path.join(cipd_cache, 'cache'), env['CIPD_CACHE_DIR'])

  def test_main_relative_cwd_no_cmd(self):
    cmd = [
      '--relative-cwd', 'a',
    ]
    with self.assertRaises(SystemExit):
      run_isolated.main(cmd)

  def test_main_bad_relative_cwd(self):
    cmd = [
      '--raw-cmd',
      '--relative-cwd', 'a/../../b',
      '--',
      'bin/echo${EXECUTABLE_SUFFIX}',
      'hello',
      'world',
    ]
    with self.assertRaises(SystemExit):
      run_isolated.main(cmd)

  def test_main_naked_with_caches(self):
    # An empty named cache is not kept!
    # Interestingly, because we would need to put something in the named cache
    # for it to be kept, we need the tool to write to it. This is tested in the
    # smoke test.
    trimmed = []
    def trim_caches(caches, root, min_free_space, max_age_secs):
      trimmed.append(True)
      self.assertEqual(2, len(caches))
      self.assertTrue(root)
      # The name cache root is increased by the sum of the two hints.
      self.assertEqual(2*1024*1024*1024 + 1100, min_free_space)
      self.assertEqual(1814400, max_age_secs)
    self.mock(local_caching, 'trim_caches', trim_caches)
    nc = os.path.join(self.tempdir, 'named_cache')
    cmd = [
        '--no-log',
        '--leak-temp-dir',
        '--cache',
        os.path.join(self.tempdir, 'isolated_cache'),
        '100',
        '--named-cache-root',
        nc,
        '--named-cache',
        'cache_foo',
        'foo',
        '100',
        '--named-cache',
        'cache_bar',
        'bar',
        '1000',
        '--raw-cmd',
        '--',
        'bin/echo${EXECUTABLE_SUFFIX}',
        'hello',
        'world',
    ]
    ret = run_isolated.main(cmd)
    self.assertEqual(0, ret)

    for cache_name in ('cache_foo', 'cache_bar'):
      named_path = os.path.join(nc, 'named', cache_name)
      self.assertFalse(fs.exists(named_path))
    self.assertTrue(trimmed)

  def test_modified_cwd(self):
    isolated = json_dumps({
        'command': ['../out/some.exe', 'arg'],
        'relative_cwd': 'some',
    })
    isolated_hash = isolateserver_fake.hash_content(isolated)
    files = {isolated_hash: isolated}
    _ = self._run_tha_test(isolated_hash, files)
    self.assertEqual(
        [
          (
            [self.ir_dir(u'out', u'some.exe'), 'arg'],
            {
              'cwd': self.ir_dir('some'),
              'detached': True,
              'close_fds': True,
              'lower_priority': False,
              'containment': None,
            },
          ),
        ],
        self.popen_calls)

  def test_python_cmd_lower_priority(self):
    isolated = json_dumps({
        'command': ['../out/cmd.py', 'arg'],
        'relative_cwd': 'some',
    })
    isolated_hash = isolateserver_fake.hash_content(isolated)
    files = {isolated_hash: isolated}
    _ = self._run_tha_test(isolated_hash, files, lower_priority=True)
    # Injects sys.executable but on macOS, the path may be different than
    # sys.executable due to symlinks.
    self.assertEqual(1, len(self.popen_calls))
    cmd, args = self.popen_calls[0]
    self.assertEqual(
        {
          'cwd': self.ir_dir('some'),
          'detached': True,
          'close_fds': True,
          'lower_priority': True,
          'containment': None,
        },
        args)
    self.assertIn('python', cmd[0])
    self.assertEqual([os.path.join(u'..', 'out', 'cmd.py'), u'arg'], cmd[1:])

  def test_run_tha_test_non_isolated(self):
    _ = self._run_tha_test(command=[u'/bin/echo', u'hello', u'world'])
    self.assertEqual(
        [
          (
            [u'/bin/echo', u'hello', u'world'],
            {
              'cwd': self.ir_dir(),
              'detached': True,
              'close_fds': True,
              'lower_priority': False,
              'containment': None,
            },
          ),
        ],
        self.popen_calls)

  def test_main_containment(self):
    def fake_wait(args, **kwargs):  # pylint: disable=unused-argument
      # Success.
      return 0
    self.popen_fakes.append(fake_wait)
    cmd = [
      '--no-log',
      '--raw-cmd',
      '--lower-priority',
      '--containment-type', 'JOB_OBJECT',
      '--limit-processes', '42',
      '--limit-total-committed-memory', '1024',
      '--',
      '/bin/echo',
      'hello',
      'world',
    ]
    ret = run_isolated.main(cmd)
    self.assertEqual(0, ret)
    self.assertEqual(
        [
          (
            ['/bin/echo', 'hello', 'world'],
            {
              'cwd': os.path.join(
                  # Necessary on macOS.
                  os.path.realpath(self.tempdir),
                  'cwd',
                  run_isolated.ISOLATED_RUN_DIR),
              'detached': True,
              'close_fds': True,
              'lower_priority': True,
              'containment': subprocess42.Containment(
                  containment_type=subprocess42.Containment.JOB_OBJECT,
                  limit_processes=42,
                  limit_total_committed_memory=1024,
              ),
            },
          ),
        ],
        self.popen_calls)

  def test_fetch_and_map_with_go(self):
    # Sanity test for run_isolated._fetch_and_map_with_go(...).
    _, options, _ = run_isolated.parse_args([])
    cipd_server = 'https://chrome-infra-packages.appspot.com/'
    storage = isolate_storage.IsolateServer(
        isolate_storage.ServerRef(cipd_server, options.namespace))

    def fake_wait(args, **kwargs):  # pylint: disable=unused-argument
      for arg in args:
        self.assertTrue(
            isinstance(arg, str) or isinstance(arg.encode('utf-8'), str), arg)

      json_path = args[args.index('-fetch-and-map-result-json') + 1]
      with open(json_path, 'w') as json_file:
        json.dump({
            'isolated': {},
            'items_cold': '',
            'items_hot': '',
        }, json_file)
      return 0

    self.popen_fakes.append(fake_wait)
    bundle, stats = run_isolated._fetch_and_map_with_go(
        'fake_isolated_hash', storage, 'fake_outdir', 'fake_cache_dir',
        local_caching.CachePolicies(0, 0, 0, 0), 'fake/path/to/isolated')
    self.assertTrue(bundle)
    self.assertTrue(stats)


class RunIsolatedTestRun(RunIsolatedTestBase):
  # Runs the actual command requested.
  def test_output(self):
    # Starts a full isolate server mock and have run_tha_test() uploads results
    # back after the task completed.
    server = isolateserver_fake.FakeIsolateServer()
    try:
      script = (
        'import sys\n'
        'open(sys.argv[1], "w").write("bar")\n')
      script_hash = isolateserver_fake.hash_content(script)
      isolated = {
          u'algo': u'sha-1',
          u'command': [u'cmd.py', u'${ISOLATED_OUTDIR}/foo'],
          u'files': {
              u'cmd.py': {
                  u'h': script_hash,
                  u'm': 0o700,
                  u's': len(script),
              },
          },
          u'version': isolated_format.ISOLATED_FILE_VERSION,
      }
      if sys.platform == 'win32':
        isolated[u'files'][u'cmd.py'].pop(u'm')
      isolated_data = json_dumps(isolated)
      isolated_hash = isolateserver_fake.hash_content(isolated_data)
      server.add_content('default-store', script)
      server.add_content('default-store', isolated_data)
      store = isolateserver.get_storage(
          isolate_storage.ServerRef(server.url, 'default-store'))

      self.mock(sys, 'stdout', StringIO.StringIO())
      data = run_isolated.TaskData(
          command=[],
          relative_cwd=None,
          extra_args=[],
          isolated_hash=isolated_hash,
          storage=store,
          isolate_cache=local_caching.MemoryContentAddressedCache(),
          outputs=None,
          install_named_caches=init_named_caches_stub,
          leak_temp_dir=False,
          root_dir=None,
          hard_timeout=60,
          grace_period=30,
          bot_file=None,
          switch_to_account=False,
          install_packages_fn=run_isolated.noop_install_packages,
          use_go_isolated=False,
          go_cache_dir=None,
          go_cache_policies=None,
          env={},
          env_prefix={},
          lower_priority=False,
          containment=None)
      ret = run_isolated.run_tha_test(data, None)
      self.assertEqual(0, ret)

      # It uploaded back. Assert the store has a new item containing foo.
      hashes = {isolated_hash, script_hash}
      output_hash = isolateserver_fake.hash_content('bar')
      hashes.add(output_hash)
      isolated = {
          u'algo': u'sha-1',
          u'files': {
              u'foo': {
                  u'h': output_hash,
                  u'm': 0o600,
                  u's': 3,
              },
          },
          u'version': isolated_format.ISOLATED_FILE_VERSION,
      }
      if sys.platform == 'win32':
        isolated[u'files'][u'foo'].pop(u'm')
      uploaded = json_dumps(isolated)
      uploaded_hash = isolateserver_fake.hash_content(uploaded)
      hashes.add(uploaded_hash)
      self.assertEqual(hashes, set(server.contents['default-store']))

      expected = ''.join([
        '[run_isolated_out_hack]',
        '{"hash":"%s","namespace":"default-store","storage":%s}' % (
            uploaded_hash, json.dumps(server.url)),
        '[/run_isolated_out_hack]'
      ]) + '\n'
      # pylint: disable=no-member
      self.assertEqual(expected, sys.stdout.getvalue())
    finally:
      server.close()


FILE, LINK, RELATIVE_LINK, DIR = range(4)


class RunIsolatedTestOutputs(RunIsolatedTestBase):
  # Unit test for link_outputs_to_outdir function.

  def create_src_tree(self, run_dir, src_dir):
    # Create files and directories specified by src_dir in run_dir.
    for path in src_dir:
      full_path = os.path.join(run_dir, path)
      (t, content) = src_dir[path]
      if t == FILE:
        open(full_path, 'w').write(content)
      elif t == RELATIVE_LINK:
        fs.symlink(content, full_path)
      elif t == LINK:
        root_dir = os.path.join(self.tempdir, 'ir')
        real_path = os.path.join(root_dir, content)
        fs.symlink(real_path, full_path)
      else:
        fs.mkdir(full_path)
        self.create_src_tree(os.path.join(run_dir, path), content)

  def assertExpectedTree(self, expected):
    # Return True is the entries in out_dir are exactly the same as entries in
    # expected. Return False otherwise.
    count = 0
    for path in expected:
      content = expected[path]
      # Assume expected path are always relative to root.
      root_dir = os.path.join(self.tempdir, 'io')
      full_path = os.path.join(root_dir, path)
      self.assertTrue(fs.exists(full_path))
      while fs.islink(full_path):
        full_path = fs.readlink(full_path)
      # If we expect a non-empty directory, check the entries in dir.
      # If we expect an empty dir, its existence (checked above) is sufficient.
      if not fs.isdir(full_path):
        with open(full_path, 'r') as f:
          self.assertEqual(f.read(), content)
      count += 1
    self.assertEqual(count, len(expected))

  def link_outputs_test(self, src_dir, outputs):
    run_dir = os.path.join(self.tempdir, 'ir')
    out_dir = os.path.join(self.tempdir, 'io')
    fs.mkdir(run_dir)
    fs.mkdir(out_dir)
    self.create_src_tree(run_dir, src_dir)
    run_isolated.link_outputs_to_outdir(run_dir, out_dir, outputs)

  def test_file(self):
    src_dir = {
        'foo_file': (FILE, 'contents of foo'),
    }
    outputs = ['foo_file']
    expected = {
        'foo_file': 'contents of foo',
    }
    self.link_outputs_test(src_dir, outputs)
    self.assertExpectedTree(expected)

  def test_symlink_to_file(self):
    src_dir = {
        'foo_file': (FILE, 'contents of foo'),
        'foo_link': (LINK, 'foo_file'),
    }
    outputs = ['foo_link']
    expected = {
        'foo_link': 'contents of foo',
    }
    self.link_outputs_test(src_dir, outputs)
    self.assertExpectedTree(expected)

  def test_dir_containing_files(self):
    src_dir = {
        'subdir': (DIR, {
            'child_a': (FILE, 'contents of a'),
            'child_b': (FILE, 'contents of b'),
        })
    }
    outputs = [os.path.join('subdir', '')]
    expected = {
        os.path.join('subdir', 'child_a'): 'contents of a',
        os.path.join('subdir', 'child_b'): 'contents of b',
    }
    self.link_outputs_test(src_dir, outputs)
    self.assertExpectedTree(expected)

  def test_relative_symlink(self):
    src_dir = {
        'foo_file': (FILE, 'contents of foo'),
        'subdir': (DIR, {
            'foo_link': (RELATIVE_LINK, '../foo_file'),
            'subsubdir': (DIR, {
                'bar_link': (RELATIVE_LINK, '../foo_link'),
            }),
        }),
    }
    outputs = [os.path.join('subdir', 'subsubdir', 'bar_link')]
    expected = {
        os.path.join('subdir', 'subsubdir', 'bar_link'): 'contents of foo',
    }
    self.link_outputs_test(src_dir, outputs)
    self.assertExpectedTree(expected)

  def test_symlink_to_dir_containing_files(self):
    src_dir = {
        'subdir_link': (LINK, 'subdir'),
        'subdir': (DIR, {
            'child_a': (FILE, 'contents of a'),
        }),
    }
    outputs = ['subdir_link']
    expected = {
        os.path.join('subdir_link', 'child_a'): 'contents of a',
    }
    self.link_outputs_test(src_dir, outputs)
    self.assertExpectedTree(expected)

  def test_symlink_to_symlink_to_dir_containing_files(self):
    src_dir = {
        'subdir_link': (LINK, 'subdir_link2'),
        'subdir_link2': (LINK, 'subdir'),
        'subdir': (DIR, {
            'child_a': (FILE, 'contents of a'),
            'child_b': (FILE, 'contents of b'),
        }),
    }
    outputs = ['subdir_link']
    expected = {
        os.path.join('subdir_link', 'child_a'): 'contents of a',
        os.path.join('subdir_link', 'child_b'): 'contents of b',
    }
    self.link_outputs_test(src_dir, outputs)
    self.assertExpectedTree(expected)

  def test_empty_dir(self):
    src_dir = {
        'subdir': (DIR, {}),
    }
    outputs = [os.path.join('subdir', '')]
    expected = {
        os.path.join('subdir', ''): '',
    }
    self.link_outputs_test(src_dir, outputs)
    self.assertExpectedTree(expected)

  def test_dir_ignore_trailing_slash(self):
    src_dir = {
        'subdir': (DIR, {}),
    }
    outputs = [os.path.join('subdir', '')]
    expected = {
        'subdir': '',
    }
    self.link_outputs_test(src_dir, outputs)
    self.assertExpectedTree(expected)

  def test_dir_containing_empty_dir(self):
    src_dir = {
        'subdir': (DIR, {
            'subsubdir': (DIR, ''),
        }),
    }
    outputs = [os.path.join('subdir', '')]
    expected = {
        os.path.join('subdir', 'subsubdir', ''): '',
        os.path.join('subdir', 'subsubdir', ''): '',
    }
    self.link_outputs_test(src_dir, outputs)
    self.assertExpectedTree(expected)

  def test_symlink_to_empty_dir(self):
    src_dir = {
        'subdir': (DIR, {}),
        'subdir_link': (LINK, 'subdir'),
    }
    outputs = ['subdir_link']
    expected = {
        os.path.join('subdir_link', ''): '',
    }
    self.link_outputs_test(src_dir, outputs)
    self.assertExpectedTree(expected)

  def test_symlink_to_nonexistent_file(self):
    src_dir = {
        'bad_link': (LINK, 'nonexistent_file'),
    }
    outputs = ['bad_link']
    expected = {}
    self.link_outputs_test(src_dir, outputs)
    self.assertExpectedTree(expected)

  def test_symlink_to_symlink_to_file(self):
    src_dir = {
        'first_link': (LINK, 'second_link'),
        'second_link': (LINK, 'foo_file'),
        'foo_file': (FILE, 'contents of foo'),
    }
    outputs = ['first_link']
    expected = {
        'first_link': 'contents of foo',
    }
    self.link_outputs_test(src_dir, outputs)
    self.assertExpectedTree(expected)

  def test_symlink_to_symlink_to_nonexistent_file(self):
    src_dir = {
        'first_link': (LINK, 'second_link'),
        'second_link': (LINK, 'nonexistent_file'),
    }
    outputs = ['first_link']
    expected = {}
    self.link_outputs_test(src_dir, outputs)
    self.assertExpectedTree(expected)

  def test_symlink_to_file_in_parent(self):
    src_dir = {
        'subdir': (DIR, {
            'subsubdir': (DIR, {
                'foo_link': (LINK, 'subdir/foo_file'),
            }),
            'foo_file': (FILE, 'contents of foo'),
        }),
    }
    outputs = [
      os.path.join('subdir', 'subsubdir', 'foo_link'),
      os.path.join('subdir', 'foo_file'),
    ]
    expected = {
        os.path.join('subdir', 'subsubdir', 'foo_link'): 'contents of foo',
        os.path.join('subdir', 'foo_file'): 'contents of foo',
    }
    self.link_outputs_test(src_dir, outputs)
    self.assertExpectedTree(expected)

  def test_symlink_to_file_in_dir(self):
    src_dir = {
        'subdir_link': (LINK, 'subdir/child_a'),
        'subdir': (DIR, {
            'child_a': (FILE, 'contents of a'),
        }),
    }
    outputs = ['subdir_link']
    expected = {
        'subdir_link': 'contents of a',
    }
    self.link_outputs_test(src_dir, outputs)
    self.assertExpectedTree(expected)

  def test_symlink_to_symlink_to_file_in_dir(self):
    src_dir = {
        'first_link': (LINK, 'subdir_link'),
        'subdir_link': (LINK, 'subdir/child_a'),
        'subdir': (DIR, {
            'child_a': (FILE, 'contents of a'),
        }),
    }
    outputs = ['first_link']
    expected = {
        'first_link': 'contents of a',
    }
    self.link_outputs_test(src_dir, outputs)
    self.assertExpectedTree(expected)


class RunIsolatedTestOutputFiles(RunIsolatedTestBase):
  # Like RunIsolatedTestRun, but ensures that specific output files
  # (as opposed to anything in $(ISOLATED_OUTDIR)) are returned.
  def _run_test(self, isolated, command, extra_args):
    # Starts a full isolate server mock and have run_tha_test() uploads results
    # back after the task completed.
    server = isolateserver_fake.FakeIsolateServer()
    try:
      # Output the following structure:
      #
      # foo1
      # foodir --> foo2_sl (symlink to "foo2_content" file)
      # bardir --> bar1
      #
      # Create the symlinks only on Linux.
      script = (
        'import os\n'
        'import sys\n'
        'open(sys.argv[1], "w").write("foo1")\n'
        'bar1_path = os.path.join(sys.argv[3], "bar1")\n'
        'open(bar1_path, "w").write("bar1")\n'
        'if sys.platform.startswith("linux"):\n'
        '  foo_realpath = os.path.abspath("foo2_content")\n'
        '  open(foo_realpath, "w").write("foo2")\n'
        '  os.symlink(foo_realpath, sys.argv[2])\n'
        'else:\n'
        '  open(sys.argv[2], "w").write("foo2")\n')
      script_hash = isolateserver_fake.hash_content(script)
      isolated['files']['cmd.py'] = {
          'h': script_hash,
          'm': 0o700,
          's': len(script),
      }
      if sys.platform == 'win32':
        isolated['files']['cmd.py'].pop('m')
      isolated_data = json_dumps(isolated)
      isolated_hash = isolateserver_fake.hash_content(isolated_data)
      server.add_content('default-store', script)
      server.add_content('default-store', isolated_data)
      store = isolateserver.get_storage(
          isolate_storage.ServerRef(server.url, 'default-store'))

      self.mock(sys, 'stdout', StringIO.StringIO())
      data = run_isolated.TaskData(
          command=command,
          relative_cwd=None,
          extra_args=extra_args,
          isolated_hash=isolated_hash,
          storage=store,
          isolate_cache=local_caching.MemoryContentAddressedCache(),
          outputs=[
              'foo1',
              # They must be in OS native path.
              os.path.join('foodir', 'foo2_sl'),
              os.path.join('bardir', ''),
          ],
          install_named_caches=init_named_caches_stub,
          leak_temp_dir=False,
          root_dir=None,
          hard_timeout=60,
          grace_period=30,
          bot_file=None,
          switch_to_account=False,
          install_packages_fn=run_isolated.noop_install_packages,
          use_go_isolated=False,
          go_cache_dir=None,
          go_cache_policies=None,
          env={},
          env_prefix={},
          lower_priority=False,
          containment=None)
      ret = run_isolated.run_tha_test(data, None)
      self.assertEqual(0, ret)

      # It uploaded back. Assert the store has a new item containing foo.
      hashes = {isolated_hash, script_hash}
      foo1_output_hash = isolateserver_fake.hash_content('foo1')
      foo2_output_hash = isolateserver_fake.hash_content('foo2')
      bar1_output_hash = isolateserver_fake.hash_content('bar1')
      hashes.add(foo1_output_hash)
      hashes.add(foo2_output_hash)
      hashes.add(bar1_output_hash)
      isolated = {
          u'algo': u'sha-1',
          u'files': {
              u'foo1': {
                  u'h': foo1_output_hash,
                  u'm': 0o600,
                  u's': 4,
              },
              os.path.join(u'foodir', 'foo2_sl'): {
                  u'h': foo2_output_hash,
                  u'm': 0o600,
                  u's': 4,
              },
              os.path.join(u'bardir', 'bar1'): {
                  u'h': bar1_output_hash,
                  u'm': 0o600,
                  u's': 4,
              },
          },
          u'version': isolated_format.ISOLATED_FILE_VERSION,
      }
      if sys.platform == 'win32':
        isolated['files']['foo1'].pop('m')
        isolated['files']['foodir\\foo2_sl'].pop('m')
        isolated['files']['bardir\\bar1'].pop('m')
      uploaded = json_dumps(isolated)
      uploaded_hash = isolateserver_fake.hash_content(uploaded)
      hashes.add(uploaded_hash)
      self.assertEqual(hashes, set(server.contents['default-store']))

      expected = ''.join([
        '[run_isolated_out_hack]',
        '{"hash":"%s","namespace":"default-store","storage":%s}' % (
            uploaded_hash, json.dumps(server.url)),
        '[/run_isolated_out_hack]'
      ]) + '\n'
      # pylint: disable=no-member
      self.assertEqual(expected, sys.stdout.getvalue())
    finally:
      server.close()

  def test_output_cmd_isolated(self):
    isolated = {
      u'algo': u'sha-1',
      u'command': [u'cmd.py', u'foo1', u'foodir/foo2_sl', 'bardir/'],
      u'files': {},
      u'version': isolated_format.ISOLATED_FILE_VERSION,
    }
    self._run_test(isolated, [], [])

  def test_output_cmd(self):
    isolated = {
      u'algo': u'sha-1',
      u'files': {},
      u'version': isolated_format.ISOLATED_FILE_VERSION,
    }
    self._run_test(
        isolated, ['cmd.py', 'foo1', 'foodir/foo2_sl', 'bardir/'], [])

  def test_output_cmd_isolated_extra_args(self):
    isolated = {
      u'algo': u'sha-1',
      u'command': [u'cmd.py'],
      u'files': {},
      u'version': isolated_format.ISOLATED_FILE_VERSION,
    }
    self._run_test(isolated, [], ['foo1', 'foodir/foo2_sl', 'bardir/'])


class RunIsolatedJsonTest(RunIsolatedTestBase):
  # Similar to RunIsolatedTest but adds the hacks to process ISOLATED_OUTDIR to
  # generate a json result file.
  def setUp(self):
    super(RunIsolatedJsonTest, self).setUp()
    self.popen_calls = []

    # pylint: disable=no-self-argument
    class Popen(object):
      def __init__(self2, args, **kwargs):
        kwargs.pop('env', None)
        self.popen_calls.append((args, kwargs))
        # Assume ${ISOLATED_OUTDIR} is the last one for testing purpose.
        self2._path = args[-1]
        self2.returncode = None

      def wait(self2, timeout=None):
        self.assertEqual(None, timeout)
        self2.returncode = 0
        with open(self2._path, 'wb') as f:
          f.write('generated data\n')
        return self2.returncode

      def kill(self):
        pass

    self.mock(subprocess42, 'Popen', Popen)

  def test_main_json(self):
    # Instruct the Popen mock to write a file in ISOLATED_OUTDIR so it will be
    # archived back on termination.
    self.mock(tools, 'disable_buffering', lambda: None)
    sub_cmd = [
      self.ir_dir(u'foo.exe'), u'cmd with space',
      '${ISOLATED_OUTDIR}/out.txt',
    ]
    isolated_in_json = json_dumps({'command': sub_cmd})
    isolated_in_hash = isolateserver_fake.hash_content(isolated_in_json)
    def get_storage(server_ref):
      return StorageFake({isolated_in_hash: isolated_in_json}, server_ref)
    self.mock(isolateserver, 'get_storage', get_storage)

    out = os.path.join(self.tempdir, 'res.json')
    cmd = [
        '--no-log',
        '--isolated',
        isolated_in_hash,
        '--cache',
        os.path.join(self.tempdir, 'isolated_cache'),
        '--isolate-server',
        'http://localhost:1',
        '--named-cache-root',
        os.path.join(self.tempdir, 'named_cache'),
        '--json',
        out,
        '--root-dir',
        self.tempdir,
    ]
    ret = run_isolated.main(cmd)
    self.assertEqual(0, ret)
    # Replace ${ISOLATED_OUTDIR} with the temporary directory.
    sub_cmd[2] = self.popen_calls[0][0][2]
    self.assertNotIn('ISOLATED_OUTDIR', sub_cmd[2])
    self.assertEqual(
        [
          (
            sub_cmd,
            {
              'cwd': self.ir_dir(),
              'detached': True,
              'close_fds': True,
              'lower_priority': False,
              'containment': subprocess42.Containment(),
            },
          ),
        ],
        self.popen_calls)
    isolated_out = {
        'algo': 'sha-1',
        'files': {
            'out.txt': {
                'h': isolateserver_fake.hash_content('generated data\n'),
                's': 15,
                'm': 0o600,
            },
        },
        'version': isolated_format.ISOLATED_FILE_VERSION,
    }
    if sys.platform == 'win32':
      del isolated_out['files']['out.txt']['m']
    isolated_out_json = json_dumps(isolated_out)
    isolated_out_hash = isolateserver_fake.hash_content(isolated_out_json)
    expected = {
        u'exit_code': 0,
        u'had_hard_timeout': False,
        u'internal_failure': None,
        u'outputs_ref': {
            u'isolated': six.text_type(isolated_out_hash),
            u'isolatedserver': u'http://localhost:1',
            u'namespace': u'default-gzip',
        },
        u'stats': {
            u'isolated': {
                u'download': {
                    u'initial_number_items': 0,
                    u'initial_size': 0,
                    u'items_cold': [len(isolated_in_json)],
                    u'items_hot': [],
                },
                u'upload': {
                    u'items_cold': [len(isolated_out_json)],
                    u'items_hot': [15],
                },
            },
        },
        u'version': 5,
    }
    actual = tools.read_json(out)
    # duration can be exactly 0 due to low timer resolution, especially but not
    # exclusively on Windows.
    self.assertLessEqual(0, actual.pop(u'duration'))
    actual_isolated_stats = actual[u'stats'][u'isolated']
    self.assertLessEqual(0, actual_isolated_stats[u'download'].pop(u'duration'))
    self.assertLessEqual(0, actual_isolated_stats[u'upload'].pop(u'duration'))
    for i in (u'download', u'upload'):
      for j in (u'items_cold', u'items_hot'):
        actual_isolated_stats[i][j] = large.unpack(
            base64.b64decode(actual_isolated_stats[i][j]))
    self.assertEqual(expected, actual)


if __name__ == '__main__':
  test_env.main()
