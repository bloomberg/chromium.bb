# Copyright 2015 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Tests for mb.py."""

import sys
import unittest

import mb


class FakeMB(mb.MetaBuildWrapper):
  def __init__(self):
    super(FakeMB, self).__init__()
    self.files = {}
    self.calls = []
    self.out = []
    self.err = []
    self.chromium_src_dir = '/fake_src'
    self.default_config = '/fake_src/tools/mb/mb_config.pyl'

  def ExpandUser(self, path):
    return '$HOME/%s' % path

  def Exists(self, path):
    return self.files.get(path) is not None

  def ReadFile(self, path):
    return self.files[path]

  def WriteFile(self, path, contents):
    self.files[path] = contents

  def Call(self, cmd):
    self.calls.append(cmd)
    return 0, '', ''

  def Print(self, *args, **kwargs):
    sep = kwargs.get('sep', ' ')
    end = kwargs.get('end', '\n')
    f = kwargs.get('file', sys.stdout)
    if f == sys.stderr:
      self.err.append(sep.join(args) + end)
    else:
      self.out.append(sep.join(args) + end)

class IntegrationTest(unittest.TestCase):
  def test_validate(self):
    # Note that this validates that the actual mb_config.pyl is valid.
    ret = mb.main(['validate', '--quiet'])
    self.assertEqual(ret, 0)


TEST_CONFIG = """\
{
  'common_dev_configs': ['gn_debug'],
  'configs': {
    'gyp_rel_bot': ['gyp', 'rel', 'goma'],
    'gn_debug': ['gn', 'debug'],
    'private': ['gyp', 'fake_feature1'],
    'unsupported': ['gn', 'fake_feature2'],
  },
  'masters': {
    'fake_master': {
      'fake_builder': 'gyp_rel_bot',
    },
  },
  'mixins': {
    'fake_feature1': {
      'gn_args': 'enable_doom_melon=true',
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
      'gyp_defines': 'goma=1 gomadir="$(goma_dir)"',
    },
    'rel': {
      'gn_args': 'is_debug=false',
      'gyp_config': 'Release',
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
  def check(self, args, files=None, cmds=None, out=None, err=None, ret=None):
    m = FakeMB()
    if files:
      for path, contents in files.items():
        m.files[path] = contents
    m.files.setdefault(mb.default_config, TEST_CONFIG)
    m.ParseArgs(args)
    actual_ret = m.args.func()
    if ret is not None:
      self.assertEqual(actual_ret, ret)
    if out is not None:
      self.assertEqual(m.out, out)
    if err is not None:
      self.assertEqual(m.err, err)
    if cmds is not None:
      self.assertEqual(m.cmds, cmds)

  def test_analyze(self):
    files = {'/tmp/in.json': '{"files": [], "targets": []}'}
    self.check(['analyze', '-c', 'gn_debug', '//out/Default',
                '/tmp/in.json', '/tmp/out.json'],
               files=files, ret=0)
    self.check(['analyze', '-c', 'gyp_rel_bot', '//out/Release',
                '/tmp/in.json', '/tmp/out.json'],
               ret=0)

  def test_gen(self):
    self.check(['gen', '-c', 'gn_debug', '//out/Default'], ret=0)
    self.check(['gen', '-c', 'gyp_rel_bot', '//out/Release'], ret=0)

  def test_help(self):
    self.assertRaises(SystemExit, self.check, ['-h'])
    self.assertRaises(SystemExit, self.check, ['help'])
    self.assertRaises(SystemExit, self.check, ['help', 'gen'])

  def test_lookup(self):
    self.check(['lookup', '-c', 'gn_debug'], ret=0)

  def test_validate(self):
    self.check(['validate'], ret=0)


if __name__ == '__main__':
  unittest.main()
