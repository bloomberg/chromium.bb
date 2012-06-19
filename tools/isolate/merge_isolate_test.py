#!/usr/bin/env python
# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import unittest

import merge_isolate
# Create shortcuts.
from merge_isolate import KEY_TRACKED, KEY_UNTRACKED


class MergeGyp(unittest.TestCase):
  def test_unknown_key(self):
    try:
      merge_isolate.verify_variables({'foo': [],})
      self.fail()
    except AssertionError:
      pass

  def test_unknown_var(self):
    try:
      merge_isolate.verify_condition({'variables': {'foo': [],}})
      self.fail()
    except AssertionError:
      pass

  def test_union(self):
    value1 = {
      'a': set(['A']),
      'b': ['B', 'C'],
      'c': 'C',
    }
    value2 = {
      'a': set(['B', 'C']),
      'b': [],
      'd': set(),
    }
    expected = {
      'a': set(['A', 'B', 'C']),
      'b': ['B', 'C'],
      'c': 'C',
      'd': set(),
    }
    self.assertEquals(expected, merge_isolate.union(value1, value2))

  def test_eval_content(self):
    try:
      # Intrinsics are not available.
      merge_isolate.eval_content('map(str, [1, 2])')
      self.fail()
    except NameError:
      pass

  def test_load_gyp_empty(self):
    self.assertEquals({}, merge_isolate.load_gyp({}, None).flatten())

  def test_load_gyp(self):
    value = {
      'variables': {
        KEY_TRACKED: ['a'],
        KEY_UNTRACKED: ['b'],
      },
      'conditions': [
        ['OS=="atari"', {
          'variables': {
            KEY_TRACKED: ['c', 'x'],
            KEY_UNTRACKED: ['d'],
            'command': ['echo', 'Hello World'],
            'read_only': True,
          },
        }, {  # else
          'variables': {
            KEY_TRACKED: ['e', 'x'],
            KEY_UNTRACKED: ['f'],
            'command': ['echo', 'You should get an Atari'],
          },
        }],
        ['OS=="amiga"', {
          'variables': {
            KEY_TRACKED: ['g'],
            'read_only': False,
          },
        }],
        ['OS=="dendy"', {
        }],
        ['OS=="coleco"', {
        }, {  # else
          'variables': {
            KEY_UNTRACKED: ['h'],
            'read_only': None,
          },
        }],
      ],
    }
    expected = {
      'amiga': {
        'command': ['echo', 'You should get an Atari'],
        KEY_TRACKED: ['a', 'e', 'g', 'x'],
        KEY_UNTRACKED: ['b', 'f', 'h'],
        'read_only': False,
      },
      'atari': {
        'command': ['echo', 'Hello World'],
        KEY_TRACKED: ['a', 'c', 'x'],
        KEY_UNTRACKED: ['b', 'd', 'h'],
        'read_only': True,
      },
      'coleco': {
        'command': ['echo', 'You should get an Atari'],
        KEY_TRACKED: ['a', 'e', 'x'],
        KEY_UNTRACKED: ['b', 'f'],
      },
      'dendy': {
        'command': ['echo', 'You should get an Atari'],
        KEY_TRACKED: ['a', 'e', 'x'],
        KEY_UNTRACKED: ['b', 'f', 'h'],
      },
    }
    self.assertEquals(expected, merge_isolate.load_gyp(value, None).flatten())

  def test_load_gyp_duplicate_command(self):
    value = {
      'variables': {
        'command': ['rm', '-rf', '/'],
      },
      'conditions': [
        ['OS=="atari"', {
          'variables': {
            'command': ['echo', 'Hello World'],
          },
        }],
      ],
    }
    try:
      merge_isolate.load_gyp(value, None)
      self.fail()
    except AssertionError:
      pass

  def test_load_gyp_no_condition(self):
    value = {
      'variables': {
        KEY_TRACKED: ['a'],
        KEY_UNTRACKED: ['b'],
      },
    }
    expected = {
      KEY_TRACKED: ['a'],
      KEY_UNTRACKED: ['b'],
    }
    actual = merge_isolate.load_gyp(value, None)
    # Flattening the whole config will discard 'None'.
    self.assertEquals({}, actual.flatten())
    self.assertEquals([None], actual.per_os.keys())
    # But the 'None' value is still available as a backup.
    self.assertEquals(expected, actual.per_os[None].flatten())

  def test_invert_map(self):
    value = {
      'amiga': {
        'command': ['echo', 'You should get an Atari'],
        KEY_TRACKED: ['a', 'e', 'g', 'x'],
        KEY_UNTRACKED: ['b', 'f', 'h'],
        'read_only': False,
      },
      'atari': {
        'command': ['echo', 'Hello World'],
        KEY_TRACKED: ['a', 'c', 'x'],
        KEY_UNTRACKED: ['b', 'd', 'h'],
        'read_only': True,
      },
      'coleco': {
        'command': ['echo', 'You should get an Atari'],
        KEY_TRACKED: ['a', 'e', 'x'],
        KEY_UNTRACKED: ['b', 'f'],
      },
      'dendy': {
        'command': ['echo', 'You should get an Atari'],
        KEY_TRACKED: ['a', 'e', 'x'],
        KEY_UNTRACKED: ['b', 'f', 'h'],
      },
    }
    expected_values = {
      'command': {
        ('echo', 'Hello World'): set(['atari']),
        ('echo', 'You should get an Atari'): set(['amiga', 'coleco', 'dendy']),
      },
      KEY_TRACKED: {
        'a': set(['amiga', 'atari', 'coleco', 'dendy']),
        'c': set(['atari']),
        'e': set(['amiga', 'coleco', 'dendy']),
        'g': set(['amiga']),
        'x': set(['amiga', 'atari', 'coleco', 'dendy']),
      },
      KEY_UNTRACKED: {
        'b': set(['amiga', 'atari', 'coleco', 'dendy']),
        'd': set(['atari']),
        'f': set(['amiga', 'coleco', 'dendy']),
        'h': set(['amiga', 'atari', 'dendy']),
      },
      'read_only': {
        None: set(['coleco', 'dendy']),
        False: set(['amiga']),
        True: set(['atari']),
      },
    }
    expected_oses = set(['amiga', 'atari', 'coleco', 'dendy'])
    actual_values, actual_oses = merge_isolate.invert_map(value)
    self.assertEquals(expected_values, actual_values)
    self.assertEquals(expected_oses, actual_oses)

  def test_reduce_inputs(self):
    values = {
      'command': {
        ('echo', 'Hello World'): set(['atari']),
        ('echo', 'You should get an Atari'): set(['amiga', 'coleco', 'dendy']),
      },
      KEY_TRACKED: {
        'a': set(['amiga', 'atari', 'coleco', 'dendy']),
        'c': set(['atari']),
        'e': set(['amiga', 'coleco', 'dendy']),
        'g': set(['amiga']),
        'x': set(['amiga', 'atari', 'coleco', 'dendy']),
      },
      KEY_UNTRACKED: {
        'b': set(['amiga', 'atari', 'coleco', 'dendy']),
        'd': set(['atari']),
        'f': set(['amiga', 'coleco', 'dendy']),
        'h': set(['amiga', 'atari', 'dendy']),
      },
      'read_only': {
        None: set(['coleco', 'dendy']),
        False: set(['amiga']),
        True: set(['atari']),
      },
    }
    oses = set(['amiga', 'atari', 'coleco', 'dendy'])
    expected_values = {
      'command': {
        ('echo', 'Hello World'): set(['atari']),
        ('echo', 'You should get an Atari'): set(['!atari']),
      },
      KEY_TRACKED: {
        'a': set([None]),
        'c': set(['atari']),
        'e': set(['!atari']),
        'g': set(['amiga']),
        'x': set([None]),
      },
      KEY_UNTRACKED: {
        'b': set([None]),
        'd': set(['atari']),
        'f': set(['!atari']),
        'h': set(['!coleco']),
      },
      'read_only': {
        None: set(['coleco', 'dendy']),
        False: set(['amiga']),
        True: set(['atari']),
      },
    }
    actual_values, actual_oses = merge_isolate.reduce_inputs(values, oses)
    self.assertEquals(expected_values, actual_values)
    self.assertEquals(oses, actual_oses)

  def test_convert_map_to_gyp(self):
    values = {
      'command': {
        ('echo', 'Hello World'): set(['atari']),
        ('echo', 'You should get an Atari'): set(['!atari']),
      },
      KEY_TRACKED: {
        'a': set([None]),
        'c': set(['atari']),
        'e': set(['!atari']),
        'g': set(['amiga']),
        'x': set([None]),
      },
      KEY_UNTRACKED: {
        'b': set([None]),
        'd': set(['atari']),
        'f': set(['!atari']),
        'h': set(['!coleco']),
      },
      'read_only': {
        None: set(['coleco', 'dendy']),
        False: set(['amiga']),
        True: set(['atari']),
      },
    }
    oses = set(['amiga', 'atari', 'coleco', 'dendy'])
    expected = {
      'variables': {
        KEY_TRACKED: ['a', 'x'],
        KEY_UNTRACKED: ['b'],
      },
      'conditions': [
        ['OS=="amiga"', {
          'variables': {
            KEY_TRACKED: ['g'],
            'read_only': False,
          },
        }],
        ['OS=="atari"', {
          'variables': {
            'command': ['echo', 'Hello World'],
            KEY_TRACKED: ['c'],
            KEY_UNTRACKED: ['d'],
            'read_only': True,
          },
        }, {
          'variables': {
            'command': ['echo', 'You should get an Atari'],
            KEY_TRACKED: ['e'],
            KEY_UNTRACKED: ['f'],
          },
        }],
        ['OS=="coleco"', {
        }, {
          'variables': {
            KEY_UNTRACKED: ['h'],
          },
        }],
      ],
    }
    self.assertEquals(expected, merge_isolate.convert_map_to_gyp(values, oses))

  def test_merge_two_empty(self):
    # Flat stay flat. Pylint is confused about union() return type.
    # pylint: disable=E1103
    actual = merge_isolate.union(
        merge_isolate.union(
          merge_isolate.Configs([], None),
          merge_isolate.load_gyp({}, None)),
        merge_isolate.load_gyp({}, None)).flatten()
    self.assertEquals({}, actual)

  def test_merge_empty(self):
    actual = merge_isolate.convert_map_to_gyp(*merge_isolate.reduce_inputs(
        *merge_isolate.invert_map({})))
    self.assertEquals({}, actual)

  def test_load_two_conditions(self):
    linux = {
      'conditions': [
        ['OS=="linux"', {
          'variables': {
            'isolate_dependency_tracked': [
              'file_linux',
              'file_common',
            ],
          },
        }],
      ],
    }
    mac = {
      'conditions': [
        ['OS=="mac"', {
          'variables': {
            'isolate_dependency_tracked': [
              'file_mac',
              'file_common',
            ],
          },
        }],
      ],
    }
    expected = {
      'linux': {
        'isolate_dependency_tracked': ['file_common', 'file_linux'],
      },
      'mac': {
        'isolate_dependency_tracked': ['file_common', 'file_mac'],
      },
    }
    # Pylint is confused about union() return type.
    # pylint: disable=E1103
    configs = merge_isolate.union(
        merge_isolate.union(
          merge_isolate.Configs([], None),
          merge_isolate.load_gyp(linux, None)),
        merge_isolate.load_gyp(mac, None)).flatten()
    self.assertEquals(expected, configs)

  def test_load_three_conditions(self):
    linux = {
      'conditions': [
        ['OS=="linux"', {
          'variables': {
            'isolate_dependency_tracked': [
              'file_linux',
              'file_common',
            ],
          },
        }],
      ],
    }
    mac = {
      'conditions': [
        ['OS=="mac"', {
          'variables': {
            'isolate_dependency_tracked': [
              'file_mac',
              'file_common',
            ],
          },
        }],
      ],
    }
    win = {
      'conditions': [
        ['OS=="win"', {
          'variables': {
            'isolate_dependency_tracked': [
              'file_win',
              'file_common',
            ],
          },
        }],
      ],
    }
    expected = {
      'linux': {
        'isolate_dependency_tracked': ['file_common', 'file_linux'],
      },
      'mac': {
        'isolate_dependency_tracked': ['file_common', 'file_mac'],
      },
      'win': {
        'isolate_dependency_tracked': ['file_common', 'file_win'],
      },
    }
    # Pylint is confused about union() return type.
    # pylint: disable=E1103
    configs = merge_isolate.union(
        merge_isolate.union(
          merge_isolate.union(
            merge_isolate.Configs([], None),
            merge_isolate.load_gyp(linux, None)),
          merge_isolate.load_gyp(mac, None)),
        merge_isolate.load_gyp(win, None)).flatten()
    self.assertEquals(expected, configs)

  def test_merge_three_conditions(self):
    values = {
      'linux': {
        'isolate_dependency_tracked': ['file_common', 'file_linux'],
      },
      'mac': {
        'isolate_dependency_tracked': ['file_common', 'file_mac'],
      },
      'win': {
        'isolate_dependency_tracked': ['file_common', 'file_win'],
      },
    }
    expected = {
      'variables': {
        'isolate_dependency_tracked': [
          'file_common',
        ],
      },
      'conditions': [
        ['OS=="linux"', {
          'variables': {
            'isolate_dependency_tracked': [
              'file_linux',
            ],
          },
        }],
        ['OS=="mac"', {
          'variables': {
            'isolate_dependency_tracked': [
              'file_mac',
            ],
          },
        }],
        ['OS=="win"', {
          'variables': {
            'isolate_dependency_tracked': [
              'file_win',
            ],
          },
        }],
      ],
    }
    actual = merge_isolate.convert_map_to_gyp(*merge_isolate.reduce_inputs(
      *merge_isolate.invert_map(values)))
    self.assertEquals(expected, actual)

  def test_configs_comment(self):
    # Pylint is confused with merge_isolate.union() return type.
    # pylint: disable=E1103
    configs = merge_isolate.union(
        merge_isolate.load_gyp({}, '# Yo dawg!\n# Chill out.\n'),
        merge_isolate.load_gyp({}, None))
    self.assertEquals('# Yo dawg!\n# Chill out.\n', configs.file_comment)

    configs = merge_isolate.union(
        merge_isolate.load_gyp({}, None),
        merge_isolate.load_gyp({}, '# Yo dawg!\n# Chill out.\n'))
    self.assertEquals('# Yo dawg!\n# Chill out.\n', configs.file_comment)

    # Only keep the first one.
    configs = merge_isolate.union(
        merge_isolate.load_gyp({}, '# Yo dawg!\n'),
        merge_isolate.load_gyp({}, '# Chill out.\n'))
    self.assertEquals('# Yo dawg!\n', configs.file_comment)

  def test_extract_comment(self):
    self.assertEquals(
        '# Foo\n# Bar\n',
        merge_isolate.extract_comment('# Foo\n# Bar\n{}'))
    self.assertEquals(
        '',
        merge_isolate.extract_comment('{}'))


if __name__ == '__main__':
  unittest.main()
