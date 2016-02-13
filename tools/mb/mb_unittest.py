#!/usr/bin/python
# Copyright 2015 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Tests for mb.py."""

import json
import StringIO
import os
import sys
import unittest

import mb


class FakeMBW(mb.MetaBuildWrapper):
  def __init__(self, win32=False):
    super(FakeMBW, self).__init__()

    # Override vars for test portability.
    if win32:
      self.chromium_src_dir = 'c:\\fake_src'
      self.default_config = 'c:\\fake_src\\tools\\mb\\mb_config.pyl'
      self.platform = 'win32'
      self.executable = 'c:\\python\\python.exe'
      self.sep = '\\'
    else:
      self.chromium_src_dir = '/fake_src'
      self.default_config = '/fake_src/tools/mb/mb_config.pyl'
      self.executable = '/usr/bin/python'
      self.platform = 'linux2'
      self.sep = '/'

    self.files = {}
    self.calls = []
    self.cmds = []
    self.cross_compile = None
    self.out = ''
    self.err = ''
    self.rmdirs = []

  def ExpandUser(self, path):
    return '$HOME/%s' % path

  def Exists(self, path):
    return self.files.get(path) is not None

  def MaybeMakeDirectory(self, path):
    self.files[path] = True

  def PathJoin(self, *comps):
    return self.sep.join(comps)

  def ReadFile(self, path):
    return self.files[path]

  def WriteFile(self, path, contents, force_verbose=False):
    self.files[path] = contents

  def Call(self, cmd, env=None, buffer_output=True):
    if env:
      self.cross_compile = env.get('GYP_CROSSCOMPILE')
    self.calls.append(cmd)
    if self.cmds:
      return self.cmds.pop(0)
    return 0, '', ''

  def Print(self, *args, **kwargs):
    sep = kwargs.get('sep', ' ')
    end = kwargs.get('end', '\n')
    f = kwargs.get('file', sys.stdout)
    if f == sys.stderr:
      self.err += sep.join(args) + end
    else:
      self.out += sep.join(args) + end

  def TempFile(self, mode='w'):
    return FakeFile(self.files)

  def RemoveFile(self, path):
    del self.files[path]

  def RemoveDirectory(self, path):
    self.rmdirs.append(path)
    files_to_delete = [f for f in self.files if f.startswith(path)]
    for f in files_to_delete:
      self.files[f] = None


class FakeFile(object):
  def __init__(self, files):
    self.name = '/tmp/file'
    self.buf = ''
    self.files = files

  def write(self, contents):
    self.buf += contents

  def close(self):
     self.files[self.name] = self.buf


TEST_CONFIG = """\
{
  'common_dev_configs': ['gn_debug'],
  'configs': {
    'gyp_rel_bot': ['gyp', 'rel', 'goma'],
    'gn_debug': ['gn', 'debug', 'goma'],
    'gyp_debug': ['gyp', 'debug'],
    'gn_rel_bot': ['gn', 'rel', 'goma'],
    'private': ['gyp', 'rel', 'fake_feature1'],
    'unsupported': ['gn', 'fake_feature2'],
  },
  'masters': {
    'fake_master': {
      'fake_builder': 'gyp_rel_bot',
      'fake_gn_builder': 'gn_rel_bot',
      'fake_gyp_builder': 'gyp_debug',
    },
  },
  'mixins': {
    'fake_feature1': {
      'gn_args': 'enable_doom_melon=true',
      'gyp_crosscompile': True,
      'gyp_defines': 'doom_melon=1',
    },
    'fake_feature2': {
      'gn_args': 'enable_doom_melon=false',
      'gyp_defaults': 'doom_melon=0',
    },
    'gyp': {'type': 'gyp'},
    'gn': {'type': 'gn'},
    'goma': {
      'gn_args': 'use_goma=true goma_dir="$(goma_dir)"',
      'gyp_defines': 'goma=1 gomadir=$(goma_dir)',
    },
    'rel': {
      'gn_args': 'is_debug=false',
    },
    'debug': {
      'gn_args': 'is_debug=true',
    },
  },
  'private_configs': ['private'],
  'unsupported_configs': ['unsupported'],
}
"""


class UnitTest(unittest.TestCase):
  def fake_mbw(self, files=None, win32=False):
    mbw = FakeMBW(win32=win32)
    mbw.files.setdefault(mbw.default_config, TEST_CONFIG)
    if files:
      for path, contents in files.items():
        mbw.files[path] = contents
    return mbw

  def check(self, args, mbw=None, files=None, out=None, err=None, ret=None):
    if not mbw:
      mbw = self.fake_mbw(files)
    mbw.ParseArgs(args)
    actual_ret = mbw.args.func()
    if ret is not None:
      self.assertEqual(actual_ret, ret)
    if out is not None:
      self.assertEqual(mbw.out, out)
    if err is not None:
      self.assertEqual(mbw.err, err)
    return mbw

  def test_clobber(self):
    files = {
      '/fake_src/out/Debug': None,
      '/fake_src/out/Debug/mb_type': None,
    }
    mbw = self.fake_mbw(files)

    # The first time we run this, the build dir doesn't exist, so no clobber.
    self.check(['gen', '-c', 'gn_debug', '//out/Debug'], mbw=mbw, ret=0)
    self.assertEqual(mbw.rmdirs, [])
    self.assertEqual(mbw.files['/fake_src/out/Debug/mb_type'], 'gn')

    # The second time we run this, the build dir exists and matches, so no
    # clobber.
    self.check(['gen', '-c', 'gn_debug', '//out/Debug'], mbw=mbw, ret=0)
    self.assertEqual(mbw.rmdirs, [])
    self.assertEqual(mbw.files['/fake_src/out/Debug/mb_type'], 'gn')

    # Now we switch build types; this should result in a clobber.
    self.check(['gen', '-c', 'gyp_debug', '//out/Debug'], mbw=mbw, ret=0)
    self.assertEqual(mbw.rmdirs, ['/fake_src/out/Debug'])
    self.assertEqual(mbw.files['/fake_src/out/Debug/mb_type'], 'gyp')

    # Now we delete mb_type; this checks the case where the build dir
    # exists but wasn't populated by mb; this should also result in a clobber.
    del mbw.files['/fake_src/out/Debug/mb_type']
    self.check(['gen', '-c', 'gyp_debug', '//out/Debug'], mbw=mbw, ret=0)
    self.assertEqual(mbw.rmdirs,
                     ['/fake_src/out/Debug', '/fake_src/out/Debug'])
    self.assertEqual(mbw.files['/fake_src/out/Debug/mb_type'], 'gyp')

  def test_gn_analyze(self):
    files = {'/tmp/in.json': """{\
               "files": ["foo/foo_unittest.cc"],
               "test_targets": ["foo_unittests", "bar_unittests"],
               "additional_compile_targets": []
             }"""}

    mbw = self.fake_mbw(files)
    mbw.Call = lambda cmd, env=None, buffer_output=True: (
        0, 'out/Default/foo_unittests\n', '')

    self.check(['analyze', '-c', 'gn_debug', '//out/Default',
                '/tmp/in.json', '/tmp/out.json'], mbw=mbw, ret=0)
    out = json.loads(mbw.files['/tmp/out.json'])
    self.assertEqual(out, {
      'status': 'Found dependency',
      'compile_targets': ['foo_unittests'],
      'test_targets': ['foo_unittests']
    })

  def test_gn_analyze_all(self):
    files = {'/tmp/in.json': """{\
               "files": ["foo/foo_unittest.cc"],
               "test_targets": ["bar_unittests"],
               "additional_compile_targets": ["all"]
             }"""}
    mbw = self.fake_mbw(files)
    mbw.Call = lambda cmd, env=None, buffer_output=True: (
        0, 'out/Default/foo_unittests\n', '')
    self.check(['analyze', '-c', 'gn_debug', '//out/Default',
                '/tmp/in.json', '/tmp/out.json'], mbw=mbw, ret=0)
    out = json.loads(mbw.files['/tmp/out.json'])
    self.assertEqual(out, {
      'status': 'Found dependency (all)',
      'compile_targets': ['all', 'bar_unittests'],
      'test_targets': ['bar_unittests'],
    })

  def test_gn_analyze_missing_file(self):
    files = {'/tmp/in.json': """{\
               "files": ["foo/foo_unittest.cc"],
               "test_targets": ["bar_unittests"],
               "additional_compile_targets": []
             }"""}
    mbw = self.fake_mbw(files)
    mbw.cmds = [
        (0, '', ''),
        (1, 'The input matches no targets, configs, or files\n', ''),
        (1, 'The input matches no targets, configs, or files\n', ''),
    ]

    self.check(['analyze', '-c', 'gn_debug', '//out/Default',
                '/tmp/in.json', '/tmp/out.json'], mbw=mbw, ret=0)
    out = json.loads(mbw.files['/tmp/out.json'])
    self.assertEqual(out, {
      'status': 'No dependency',
      'compile_targets': [],
      'test_targets': [],
    })

  def test_gn_gen(self):
    self.check(['gen', '-c', 'gn_debug', '//out/Default', '-g', '/goma'],
               ret=0,
               out=('/fake_src/buildtools/linux64/gn gen //out/Default '
                    '\'--args=is_debug=true use_goma=true goma_dir="/goma"\' '
                    '--check\n'))

    mbw = self.fake_mbw(win32=True)
    self.check(['gen', '-c', 'gn_debug', '-g', 'c:\\goma', '//out/Debug'],
               mbw=mbw, ret=0,
               out=('c:\\fake_src\\buildtools\\win\\gn.exe gen //out/Debug '
                    '"--args=is_debug=true use_goma=true goma_dir=\\"'
                    'c:\\goma\\"" --check\n'))


  def test_gn_gen_fails(self):
    mbw = self.fake_mbw()
    mbw.Call = lambda cmd, env=None, buffer_output=True: (1, '', '')
    self.check(['gen', '-c', 'gn_debug', '//out/Default'], mbw=mbw, ret=1)

  def test_gn_gen_swarming(self):
    files = {
      '/tmp/swarming_targets': 'base_unittests\n',
      '/fake_src/testing/buildbot/gn_isolate_map.pyl': (
          "{'base_unittests': {"
          "  'label': '//base:base_unittests',"
          "  'type': 'raw',"
          "  'args': [],"
          "}}\n"
      ),
      '/fake_src/out/Default/base_unittests.runtime_deps': (
          "base_unittests\n"
      ),
    }
    mbw = self.fake_mbw(files)
    self.check(['gen',
                '-c', 'gn_debug',
                '--swarming-targets-file', '/tmp/swarming_targets',
                '//out/Default'], mbw=mbw, ret=0)
    self.assertIn('/fake_src/out/Default/base_unittests.isolate',
                  mbw.files)
    self.assertIn('/fake_src/out/Default/base_unittests.isolated.gen.json',
                  mbw.files)

  def test_gn_isolate(self):
    files = {
      '/fake_src/testing/buildbot/gn_isolate_map.pyl': (
          "{'base_unittests': {"
          "  'label': '//base:base_unittests',"
          "  'type': 'raw',"
          "  'args': [],"
          "}}\n"
      ),
      '/fake_src/out/Default/base_unittests.runtime_deps': (
          "base_unittests\n"
      ),
    }
    self.check(['isolate', '-c', 'gn_debug', '//out/Default', 'base_unittests'],
               files=files, ret=0)

    # test running isolate on an existing build_dir
    files['/fake_src/out/Default/args.gn'] = 'is_debug = True\n'
    self.check(['isolate', '//out/Default', 'base_unittests'],
               files=files, ret=0)

    files['/fake_src/out/Default/mb_type'] = 'gn\n'
    self.check(['isolate', '//out/Default', 'base_unittests'],
               files=files, ret=0)

  def test_gn_run(self):
    files = {
      '/fake_src/testing/buildbot/gn_isolate_map.pyl': (
          "{'base_unittests': {"
          "  'label': '//base:base_unittests',"
          "  'type': 'raw',"
          "  'args': [],"
          "}}\n"
      ),
      '/fake_src/out/Default/base_unittests.runtime_deps': (
          "base_unittests\n"
      ),
    }
    self.check(['run', '-c', 'gn_debug', '//out/Default', 'base_unittests'],
               files=files, ret=0)

  def test_gn_lookup(self):
    self.check(['lookup', '-c', 'gn_debug'], ret=0)

  def test_gn_lookup_goma_dir_expansion(self):
    self.check(['lookup', '-c', 'gn_rel_bot', '-g', '/foo'], ret=0,
               out=("/fake_src/buildtools/linux64/gn gen _path_ "
                    "'--args=is_debug=false use_goma=true "
                    "goma_dir=\"/foo\"'\n" ))

  def test_gyp_analyze(self):
    mbw = self.check(['analyze', '-c', 'gyp_rel_bot', '//out/Release',
                     '/tmp/in.json', '/tmp/out.json'],
                     ret=0)
    self.assertIn('analyzer', mbw.calls[0])

  def test_gyp_crosscompile(self):
    mbw = self.fake_mbw()
    self.check(['gen', '-c', 'private', '//out/Release'], mbw=mbw)
    self.assertTrue(mbw.cross_compile)

  def test_gyp_gen(self):
    self.check(['gen', '-c', 'gyp_rel_bot', '-g', '/goma', '//out/Release'],
               ret=0,
               out=("GYP_DEFINES='goma=1 gomadir=/goma'\n"
                    "python build/gyp_chromium -G output_dir=out\n"))

    mbw = self.fake_mbw(win32=True)
    self.check(['gen', '-c', 'gyp_rel_bot', '-g', 'c:\\goma', '//out/Release'],
               mbw=mbw, ret=0,
               out=("set GYP_DEFINES=goma=1 gomadir='c:\\goma'\n"
                    "python build\\gyp_chromium -G output_dir=out\n"))

  def test_gyp_gen_fails(self):
    mbw = self.fake_mbw()
    mbw.Call = lambda cmd, env=None, buffer_output=True: (1, '', '')
    self.check(['gen', '-c', 'gyp_rel_bot', '//out/Release'], mbw=mbw, ret=1)

  def test_gyp_lookup_goma_dir_expansion(self):
    self.check(['lookup', '-c', 'gyp_rel_bot', '-g', '/foo'], ret=0,
               out=("GYP_DEFINES='goma=1 gomadir=/foo'\n"
                    "python build/gyp_chromium -G output_dir=_path_\n"))

  def test_help(self):
    orig_stdout = sys.stdout
    try:
      sys.stdout = StringIO.StringIO()
      self.assertRaises(SystemExit, self.check, ['-h'])
      self.assertRaises(SystemExit, self.check, ['help'])
      self.assertRaises(SystemExit, self.check, ['help', 'gen'])
    finally:
      sys.stdout = orig_stdout


  def test_validate(self):
    self.check(['validate'], ret=0)


if __name__ == '__main__':
  unittest.main()
