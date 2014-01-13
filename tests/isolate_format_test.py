#!/usr/bin/env python
# Copyright 2014 The Swarming Authors. All rights reserved.
# Use of this source code is governed under the Apache License, Version 2.0 that
# can be found in the LICENSE file.

import cStringIO
import logging
import os
import sys
import tempfile
import unittest

ROOT_DIR = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, ROOT_DIR)

FAKE_DIR = u'/path/to/non_existing'

import isolate_format
import run_isolated
from utils import tools
# Create shortcuts.
from isolate_format import KEY_TOUCHED, KEY_TRACKED, KEY_UNTRACKED


# Access to a protected member XXX of a client class
# pylint: disable=W0212


class IsolateFormatTest(unittest.TestCase):
  def test_unknown_key(self):
    try:
      isolate_format.verify_variables({'foo': [],})
      self.fail()
    except AssertionError:
      pass

  def test_unknown_var(self):
    try:
      isolate_format.verify_condition({'variables': {'foo': [],}}, {})
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
    self.assertEqual(expected, isolate_format.union(value1, value2))

  def test_eval_content(self):
    try:
      # Intrinsics are not available.
      isolate_format.eval_content('map(str, [1, 2])')
      self.fail()
    except NameError:
      pass

  def test_load_isolate_as_config_empty(self):
    self.assertEqual({}, isolate_format.load_isolate_as_config(
        FAKE_DIR, {}, None).flatten())

  def test_load_isolate_as_config(self):
    value = {
      'conditions': [
        ['OS=="amiga" or OS=="atari" or OS=="coleco" or OS=="dendy"', {
          'variables': {
            KEY_TRACKED: ['a'],
            KEY_UNTRACKED: ['b'],
            KEY_TOUCHED: ['touched'],
          },
        }],
        ['OS=="atari"', {
          'variables': {
            KEY_TRACKED: ['c', 'x'],
            KEY_UNTRACKED: ['d'],
            KEY_TOUCHED: ['touched_a'],
            'command': ['echo', 'Hello World'],
            'read_only': 2,
          },
        }],
        ['OS=="amiga" or OS=="coleco" or OS=="dendy"', {
          'variables': {
            KEY_TRACKED: ['e', 'x'],
            KEY_UNTRACKED: ['f'],
            KEY_TOUCHED: ['touched_e'],
            'command': ['echo', 'You should get an Atari'],
          },
        }],
        ['OS=="amiga"', {
          'variables': {
            KEY_TRACKED: ['g'],
            'read_only': 1,
          },
        }],
        ['OS=="amiga" or OS=="atari" or OS=="dendy"', {
          'variables': {
            KEY_UNTRACKED: ['h'],
          },
        }],
      ],
    }
    expected = {
      ('amiga',): {
        'command': ['echo', 'You should get an Atari'],
        KEY_TOUCHED: ['touched', 'touched_e'],
        KEY_TRACKED: ['a', 'e', 'g', 'x'],
        KEY_UNTRACKED: ['b', 'f', 'h'],
        'read_only': 1,
      },
      ('atari',): {
        'command': ['echo', 'Hello World'],
        KEY_TOUCHED: ['touched', 'touched_a'],
        KEY_TRACKED: ['a', 'c', 'x'],
        KEY_UNTRACKED: ['b', 'd', 'h'],
        'read_only': 2,
      },
      ('coleco',): {
        'command': ['echo', 'You should get an Atari'],
        KEY_TOUCHED: ['touched', 'touched_e'],
        KEY_TRACKED: ['a', 'e', 'x'],
        KEY_UNTRACKED: ['b', 'f'],
      },
      ('dendy',): {
        'command': ['echo', 'You should get an Atari'],
        KEY_TOUCHED: ['touched', 'touched_e'],
        KEY_TRACKED: ['a', 'e', 'x'],
        KEY_UNTRACKED: ['b', 'f', 'h'],
      },
    }
    self.assertEqual(
        expected, isolate_format.load_isolate_as_config(
            FAKE_DIR, value, None).flatten())

  def test_load_isolate_as_config_duplicate_command(self):
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
      isolate_format.load_isolate_as_config(FAKE_DIR, value, None)
      self.fail()
    except AssertionError:
      pass

  def test_invert_map(self):
    value = {
      ('amiga',): {
        'command': ['echo', 'You should get an Atari'],
        KEY_TOUCHED: ['touched', 'touched_e'],
        KEY_TRACKED: ['a', 'e', 'g', 'x'],
        KEY_UNTRACKED: ['b', 'f', 'h'],
        'read_only': 0,
      },
      ('atari',): {
        'command': ['echo', 'Hello World'],
        KEY_TOUCHED: ['touched', 'touched_a'],
        KEY_TRACKED: ['a', 'c', 'x'],
        KEY_UNTRACKED: ['b', 'd', 'h'],
        'read_only': 1,
      },
      ('coleco',): {
        'command': ['echo', 'You should get an Atari'],
        KEY_TOUCHED: ['touched', 'touched_e'],
        KEY_TRACKED: ['a', 'e', 'x'],
        KEY_UNTRACKED: ['b', 'f'],
      },
      ('dendy',): {
        'command': ['echo', 'You should get an Atari'],
        KEY_TOUCHED: ['touched', 'touched_e'],
        KEY_TRACKED: ['a', 'e', 'x'],
        KEY_UNTRACKED: ['b', 'f', 'h'],
      },
    }
    amiga, atari, coleco, dendy = (
        set([(os,)]) for os in ('amiga', 'atari', 'coleco', 'dendy'))
    expected_values = {
      'command': {
        ('echo', 'Hello World'): atari,
        ('echo', 'You should get an Atari'): amiga | coleco | dendy,
      },
      KEY_TRACKED: {
        'a': amiga | atari | coleco | dendy,
        'c': atari,
        'e': amiga | coleco | dendy,
        'g': amiga,
        'x': amiga | atari | coleco | dendy,
      },
      KEY_UNTRACKED: {
        'b': amiga | atari | coleco | dendy,
        'd': atari,
        'f': amiga | coleco | dendy,
        'h': amiga | atari | dendy,
      },
      KEY_TOUCHED: {
        'touched': amiga | atari | coleco | dendy,
        'touched_a': atari,
        'touched_e': amiga | coleco | dendy,
      },
      'read_only': {
        0: amiga,
        1: atari,
      },
    }
    actual_values = isolate_format.invert_map(value)
    self.assertEqual(expected_values, actual_values)

  def test_reduce_inputs(self):
    amiga, atari, coleco, dendy = (
        set([(os,)]) for os in ('amiga', 'atari', 'coleco', 'dendy'))
    values = {
      'command': {
        ('echo', 'Hello World'): atari,
        ('echo', 'You should get an Atari'): amiga | coleco | dendy,
      },
      KEY_TRACKED: {
        'a': amiga | atari | coleco | dendy,
        'c': atari,
        'e': amiga | coleco | dendy,
        'g': amiga,
        'x': amiga | atari | coleco | dendy,
      },
      KEY_UNTRACKED: {
        'b': amiga | atari | coleco | dendy,
        'd': atari,
        'f': amiga | coleco | dendy,
        'h': amiga | atari | dendy,
      },
      KEY_TOUCHED: {
        'touched': amiga | atari | coleco | dendy,
        'touched_a': atari,
        'touched_e': amiga | coleco | dendy,
      },
      'read_only': {
        0: amiga,
        1: atari,
      },
    }
    expected_values = {
      'command': {
        ('echo', 'Hello World'): atari,
        ('echo', 'You should get an Atari'): amiga | coleco | dendy,
      },
      KEY_TRACKED: {
        'a': amiga | atari | coleco | dendy,
        'c': atari,
        'e': amiga | coleco | dendy,
        'g': amiga,
        'x': amiga | atari | coleco | dendy,
      },
      KEY_UNTRACKED: {
        'b': amiga | atari | coleco | dendy,
        'd': atari,
        'f': amiga | coleco | dendy,
        'h': amiga | atari | dendy,
      },
      KEY_TOUCHED: {
        'touched': amiga | atari | coleco | dendy,
        'touched_a': atari,
        'touched_e': amiga | coleco | dendy,
      },
      'read_only': {
        0: amiga,
        1: atari,
      },
    }
    actual_values = isolate_format.reduce_inputs(values)
    self.assertEqual(expected_values, actual_values)

  def test_reduce_inputs_merge_subfolders_and_files(self):
    linux, mac, win = (set([(os,)]) for os in ('linux', 'mac', 'win'))
    values = {
      KEY_TRACKED: {
        'folder/tracked_file': win,
        'folder_helper/tracked_file': win,
      },
      KEY_UNTRACKED: {
        'folder/': linux | mac | win,
        'folder/subfolder/': win,
        'folder/untracked_file': linux | mac | win,
        'folder_helper/': linux,
      },
      KEY_TOUCHED: {
        'folder/touched_file': win,
        'folder/helper_folder/deep_file': win,
        'folder_helper/touched_file1': mac | win,
        'folder_helper/touched_file2': linux,
      },
    }
    expected_values = {
      'command': {},
      KEY_TRACKED: {
        'folder_helper/tracked_file': win,
      },
      KEY_UNTRACKED: {
        'folder/': linux | mac | win,
        'folder_helper/': linux,
      },
      KEY_TOUCHED: {
        'folder_helper/touched_file1': mac | win,
      },
      'read_only': {},
    }
    actual_values = isolate_format.reduce_inputs(values)
    self.assertEqual(expected_values, actual_values)

  def test_reduce_inputs_take_strongest_dependency(self):
    amiga, atari, coleco, dendy = (
        set([(os,)]) for os in ('amiga', 'atari', 'coleco', 'dendy'))
    values = {
      'command': {
        ('echo', 'Hello World'): atari,
        ('echo', 'You should get an Atari'): amiga | coleco | dendy,
      },
      KEY_TRACKED: {
        'a': amiga | atari | coleco | dendy,
        'b': amiga | atari | coleco,
      },
      KEY_UNTRACKED: {
        'c': amiga | atari | coleco | dendy,
        'd': amiga | coleco | dendy,
      },
      KEY_TOUCHED: {
        'a': amiga | atari | coleco | dendy,
        'b': atari | coleco | dendy,
        'c': amiga | atari | coleco | dendy,
        'd': atari | coleco | dendy,
      },
    }
    expected_values = {
      'command': {
        ('echo', 'Hello World'): atari,
        ('echo', 'You should get an Atari'): amiga | coleco | dendy,
      },
      KEY_TRACKED: {
        'a': amiga | atari | coleco | dendy,
        'b': amiga | atari | coleco,
      },
      KEY_UNTRACKED: {
        'c': amiga | atari | coleco | dendy,
        'd': amiga | coleco | dendy,
      },
      KEY_TOUCHED: {
        'b': dendy,
        'd': atari,
      },
      'read_only': {},
    }
    actual_values = isolate_format.reduce_inputs(values)
    self.assertEqual(expected_values, actual_values)

  def test_convert_map_to_isolate_dict(self):
    amiga = ('amiga',)
    atari = ('atari',)
    coleco = ('coleco',)
    dendy = ('dendy',)
    values = {
      'command': {
        ('echo', 'Hello World'): (atari,),
        ('echo', 'You should get an Atari'): (amiga, coleco, dendy),
      },
      KEY_TRACKED: {
        'a': (amiga, atari, coleco, dendy),
        'c': (atari,),
        'e': (amiga, coleco, dendy),
        'g': (amiga,),
        'x': (amiga, atari, coleco, dendy),
      },
      KEY_UNTRACKED: {
        'b': (amiga, atari, coleco, dendy),
        'd': (atari,),
        'f': (amiga, coleco, dendy),
        'h': (amiga, atari, dendy),
      },
      KEY_TOUCHED: {
        'touched': (amiga, atari, coleco, dendy),
        'touched_a': (atari,),
        'touched_e': (amiga, coleco, dendy),
      },
      'read_only': {
        0: (amiga,),
        1: (atari,),
      },
    }
    expected_conditions = [
      ['OS=="amiga"', {
        'variables': {
          KEY_TRACKED: ['g'],
          'read_only': 0,
        },
      }],
      ['OS=="amiga" or OS=="atari" or OS=="coleco" or OS=="dendy"', {
        'variables': {
          KEY_TRACKED: ['a', 'x'],
          KEY_UNTRACKED: ['b'],
          KEY_TOUCHED: ['touched'],
        },
      }],
      ['OS=="amiga" or OS=="atari" or OS=="dendy"', {
        'variables': {
          KEY_UNTRACKED: ['h'],
        },
      }],
      ['OS=="amiga" or OS=="coleco" or OS=="dendy"', {
        'variables': {
          'command': ['echo', 'You should get an Atari'],
          KEY_TRACKED: ['e'],
          KEY_UNTRACKED: ['f'],
          KEY_TOUCHED: ['touched_e'],
        },
      }],
      ['OS=="atari"', {
        'variables': {
          'command': ['echo', 'Hello World'],
          KEY_TRACKED: ['c'],
          KEY_UNTRACKED: ['d'],
          KEY_TOUCHED: ['touched_a'],
          'read_only': 1,
        },
      }],
    ]
    actual = isolate_format.convert_map_to_isolate_dict(values, ('OS',))
    self.assertEqual(expected_conditions, sorted(actual.pop('conditions')))
    self.assertFalse(actual)

  def test_merge_two_empty(self):
    # Flat stay flat. Pylint is confused about union() return type.
    # pylint: disable=E1103
    actual = isolate_format.union(
        isolate_format.union(
          isolate_format.Configs(None, ()),
          isolate_format.load_isolate_as_config(FAKE_DIR, {}, None)),
        isolate_format.load_isolate_as_config(FAKE_DIR, {}, None)).flatten()
    self.assertEqual({}, actual)

  def test_merge_empty(self):
    actual = isolate_format.convert_map_to_isolate_dict(
        isolate_format.reduce_inputs(isolate_format.invert_map({})),
        ('dummy1', 'dummy2'))
    self.assertEqual({'conditions': []}, actual)

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
      ('linux',): {
        'isolate_dependency_tracked': ['file_common', 'file_linux'],
      },
      ('mac',): {
        'isolate_dependency_tracked': ['file_common', 'file_mac'],
      },
    }
    # Pylint is confused about union() return type.
    # pylint: disable=E1103
    configs = isolate_format.union(
        isolate_format.union(
          isolate_format.Configs(None, ()),
          isolate_format.load_isolate_as_config(FAKE_DIR, linux, None)),
        isolate_format.load_isolate_as_config(FAKE_DIR, mac, None)).flatten()
    self.assertEqual(expected, configs)

  def test_load_three_conditions(self):
    linux = {
      'conditions': [
        ['OS=="linux" and chromeos==1', {
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
        ['OS=="mac" and chromeos==0', {
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
        ['OS=="win" and chromeos==0', {
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
      ('linux', 1): {
        'isolate_dependency_tracked': ['file_common', 'file_linux'],
      },
      ('mac', 0): {
        'isolate_dependency_tracked': ['file_common', 'file_mac'],
      },
      ('win', 0): {
        'isolate_dependency_tracked': ['file_common', 'file_win'],
      },
    }
    # Pylint is confused about union() return type.
    # pylint: disable=E1103
    configs = isolate_format.union(
        isolate_format.union(
          isolate_format.union(
            isolate_format.Configs(None, ()),
            isolate_format.load_isolate_as_config(FAKE_DIR, linux, None)),
          isolate_format.load_isolate_as_config(FAKE_DIR, mac, None)),
        isolate_format.load_isolate_as_config(FAKE_DIR, win, None))
    self.assertEqual(expected, configs.flatten())

  def test_safe_index(self):
    self.assertEqual(1, isolate_format._safe_index(('a', 'b'), 'b'))
    self.assertEqual(None, isolate_format._safe_index(('a', 'b'), 'c'))

  def test_get_map_keys(self):
    self.assertEqual(
        (0, None, 1), isolate_format._get_map_keys(('a', 'b', 'c'), ('a', 'c')))

  def test_map_keys(self):
    self.assertEqual(
        ('a', None, 'c'),
        isolate_format._map_keys((0, None, 1), ('a', 'c')))

  def test_load_multi_variables(self):
    # Load an .isolate with different condition on different variables.
    data = {
      'conditions': [
        ['OS=="abc"', {
          'variables': {
            'command': ['bar'],
          },
        }],
        ['CHROMEOS=="1"', {
          'variables': {
            'command': ['foo'],
          },
        }],
      ],
    }
    configs = isolate_format.load_isolate_as_config(FAKE_DIR, data, None)
    self.assertEqual(('CHROMEOS', 'OS'), configs.config_variables)
    flatten = dict((k, v.flatten()) for k, v in configs._by_config.iteritems())
    expected = {
        ((None, 'abc')): {'command': ['bar']},
        (('1', None)): {'command': ['foo']},
        # TODO(maruel): It is a conflict.
        (('1', 'abc')): {'command': ['bar']},
    }
    self.assertEqual(expected, flatten)

  def test_union_multi_variables(self):
    data1 = {
      'conditions': [
        ['OS=="abc"', {
          'variables': {
            'command': ['bar'],
          },
        }],
      ],
    }
    data2 = {
      'conditions': [
        ['CHROMEOS=="1"', {
          'variables': {
            'command': ['foo'],
          },
        }],
      ],
    }
    configs1 = isolate_format.load_isolate_as_config(FAKE_DIR, data1, None)
    configs2 = isolate_format.load_isolate_as_config(FAKE_DIR, data2, None)
    configs = configs1.union(configs2)
    self.assertEqual(('CHROMEOS', 'OS'), configs.config_variables)
    flatten = dict((k, v.flatten()) for k, v in configs._by_config.iteritems())
    expected = {
        ((None, 'abc')): {'command': ['bar']},
        ('1', None): {'command': ['foo']},
    }
    self.assertEqual(expected, flatten)

  def test_make_isolate_multi_variables(self):
    config = isolate_format.Configs(None, ('CHROMEOS', 'OS'))
    config._by_config[(('0', 'linux'))] = isolate_format.ConfigSettings(
        {'command': ['bar']})
    config._by_config[(('1', 'linux'))] = isolate_format.ConfigSettings(
        {'command': ['foo']})
    expected = {
      'conditions': [
        ['CHROMEOS=="0" and OS=="linux"', {
          'variables': {
            'command': ['bar'],
          },
        }],
        ['CHROMEOS=="1" and OS=="linux"', {
          'variables': {
            'command': ['foo'],
          },
        }],
      ],
    }
    self.assertEqual(expected, config.make_isolate_file())

  def test_make_isolate_multi_variables_missing(self):
    config = isolate_format.Configs(None, ('CHROMEOS', 'OS'))
    config._by_config[((None, 'abc'))] = isolate_format.ConfigSettings(
        {'command': ['bar']})
    config._by_config[(('1', None))] = isolate_format.ConfigSettings(
        {'command': ['foo']})
    expected = {
      'conditions': [
        ['CHROMEOS=="1"', {
          'variables': {
            'command': ['foo'],
          },
        }],
        ['OS=="abc"', {
          'variables': {
            'command': ['bar'],
          },
        }],
      ],
    }
    self.assertEqual(expected, config.make_isolate_file())

  def test_make_isolate_4_variables(self):
    # Test multiple combinations of bound and free variables.
    config = isolate_format.Configs(None, ('CHROMEOS', 'OS', 'BRAND', 'LIB'))
    config._by_config = {
        (0, 'linux', None, 's'): isolate_format.ConfigSettings(
            {'command': ['bar']}),
        (None, 'mac', None, 's'): isolate_format.ConfigSettings(
            {'command': ['foo']}),
        (None, 'win', None, 's'): isolate_format.ConfigSettings(
            {'command': ['ziz']}),
        (0, 'win', 'Chrome', 's'): isolate_format.ConfigSettings(
            {'command': ['baz']}),
    }
    expected = {
      'conditions': [
        ['CHROMEOS==0 and OS=="linux" and LIB=="s"', {
          'variables': {
            'command': ['bar'],
          },
        }],
        # TODO(maruel): Short by key name.
        ['CHROMEOS==0 and OS=="win" and BRAND=="Chrome" and LIB=="s"', {
          'variables': {
            'command': ['baz'],
          },
        }],
        ['OS=="mac" and LIB=="s"', {
          'variables': {
            'command': ['foo'],
          },
        }],
        ['OS=="win" and LIB=="s"', {
          'variables': {
            'command': ['ziz'],
          },
        }],
      ],
    }
    self.assertEqual(expected, config.make_isolate_file())

  def test_merge_three_conditions(self):
    values = {
      ('linux',): {
        'isolate_dependency_tracked': ['file_common', 'file_linux'],
      },
      ('mac',): {
        'isolate_dependency_tracked': ['file_common', 'file_mac'],
      },
      ('win',): {
        'isolate_dependency_tracked': ['file_common', 'file_win'],
      },
    }
    expected = {
      'conditions': [
        ['OS=="linux"', {
          'variables': {
            'isolate_dependency_tracked': [
              'file_linux',
            ],
          },
        }],
        ['OS=="linux" or OS=="mac" or OS=="win"', {
          'variables': {
            'isolate_dependency_tracked': [
              'file_common',
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
    actual = isolate_format.convert_map_to_isolate_dict(
        isolate_format.reduce_inputs(isolate_format.invert_map(values)),
        ('OS',))
    self.assertEqual(expected, actual)

  def test_merge_three_conditions_read_only(self):
    values = {
      ('linux',): {
        'isolate_dependency_tracked': ['file_common', 'file_linux'],
        'read_only': 1,
      },
      ('mac',): {
        'isolate_dependency_tracked': ['file_common', 'file_mac'],
        'read_only': 0,
      },
      ('win',): {
        'isolate_dependency_tracked': ['file_common', 'file_win'],
        'read_only': 2,
      },
      ('amiga',): {
        'read_only': 1,
      }
    }
    expected = {
      'conditions': [
        ['OS=="amiga" or OS=="linux"', {
          'variables': {
            'read_only': 1,
          },
        }],
        ['OS=="linux"', {
          'variables': {
            'isolate_dependency_tracked': [
              'file_linux',
            ],
          },
        }],
        ['OS=="linux" or OS=="mac" or OS=="win"', {
          'variables': {
            'isolate_dependency_tracked': [
              'file_common',
            ],
          },
        }],
        ['OS=="mac"', {
          'variables': {
            'isolate_dependency_tracked': [
              'file_mac',
            ],
            'read_only': 0,
          },
        }],
        ['OS=="win"', {
          'variables': {
            'isolate_dependency_tracked': [
              'file_win',
            ],
            'read_only': 2,
          },
        }],
      ],
    }
    actual = isolate_format.convert_map_to_isolate_dict(
        isolate_format.reduce_inputs(isolate_format.invert_map(values)),
        ('OS',))
    self.assertEqual(expected, actual)

  def test_configs_comment(self):
    # Pylint is confused with isolate_format.union() return type.
    # pylint: disable=E1103
    configs = isolate_format.union(
        isolate_format.load_isolate_as_config(
            FAKE_DIR, {}, '# Yo dawg!\n# Chill out.\n'),
        isolate_format.load_isolate_as_config(FAKE_DIR, {}, None))
    self.assertEqual('# Yo dawg!\n# Chill out.\n', configs.file_comment)

    configs = isolate_format.union(
        isolate_format.load_isolate_as_config(FAKE_DIR, {}, None),
        isolate_format.load_isolate_as_config(
            FAKE_DIR, {}, '# Yo dawg!\n# Chill out.\n'))
    self.assertEqual('# Yo dawg!\n# Chill out.\n', configs.file_comment)

    # Only keep the first one.
    configs = isolate_format.union(
        isolate_format.load_isolate_as_config(FAKE_DIR, {}, '# Yo dawg!\n'),
        isolate_format.load_isolate_as_config(FAKE_DIR, {}, '# Chill out.\n'))
    self.assertEqual('# Yo dawg!\n', configs.file_comment)

  def test_extract_comment(self):
    self.assertEqual(
        '# Foo\n# Bar\n', isolate_format.extract_comment('# Foo\n# Bar\n{}'))
    self.assertEqual('', isolate_format.extract_comment('{}'))

  def _test_pretty_print_impl(self, value, expected):
    actual = cStringIO.StringIO()
    isolate_format.pretty_print(value, actual)
    self.assertEqual(expected, actual.getvalue())

  def test_pretty_print_empty(self):
    self._test_pretty_print_impl({}, '{\n}\n')

  def test_pretty_print_mid_size(self):
    value = {
      'variables': {
        'bar': [
          'file1',
          'file2',
        ],
      },
      'conditions': [
        ['OS=\"foo\"', {
          'variables': {
            KEY_UNTRACKED: [
              'dir1',
              'dir2',
            ],
            KEY_TRACKED: [
              'file4',
              'file3',
            ],
            'command': ['python', '-c', 'print "H\\i\'"'],
            'read_only': 2,
            'relative_cwd': 'isol\'at\\e',
          },
        }],
        ['OS=\"bar\"', {
          'variables': {},
        }, {
          'variables': {},
        }],
      ],
    }
    expected = (
        "{\n"
        "  'variables': {\n"
        "    'bar': [\n"
        "      'file1',\n"
        "      'file2',\n"
        "    ],\n"
        "  },\n"
        "  'conditions': [\n"
        "    ['OS=\"foo\"', {\n"
        "      'variables': {\n"
        "        'command': [\n"
        "          'python',\n"
        "          '-c',\n"
        "          'print \"H\\i\'\"',\n"
        "        ],\n"
        "        'relative_cwd': 'isol\\'at\\\\e',\n"
        "        'read_only': 2\n"
        "        'isolate_dependency_tracked': [\n"
        "          'file4',\n"
        "          'file3',\n"
        "        ],\n"
        "        'isolate_dependency_untracked': [\n"
        "          'dir1',\n"
        "          'dir2',\n"
        "        ],\n"
        "      },\n"
        "    }],\n"
        "    ['OS=\"bar\"', {\n"
        "      'variables': {\n"
        "      },\n"
        "    }, {\n"
        "      'variables': {\n"
        "      },\n"
        "    }],\n"
        "  ],\n"
        "}\n")
    self._test_pretty_print_impl(value, expected)

  def test_convert_old_to_new_bypass(self):
    isolate_not_needing_conversion = {
      'conditions': [
        ['OS=="mac"', {'variables': {'foo': 'bar'}}],
        ['condition shouldn\'t matter', {'variables': {'x': 'y'}}],
      ],
    }
    self.assertEqual(
        isolate_not_needing_conversion,
        isolate_format.convert_old_to_new_format(
            isolate_not_needing_conversion))

  def test_convert_old_to_new_else(self):
    isolate_with_else_clauses = {
      'conditions': [
        ['OS=="mac"', {
          'variables': {'foo': 'bar'},
        }, {
          'variables': {'x': 'y'},
        }],
        ['OS=="foo"', {
        }, {
          'variables': {'p': 'q'},
        }],
      ],
    }
    expected_output = {
      'conditions': [
        ['OS=="foo" or OS=="linux" or OS=="win"', {
          'variables': {'x': 'y'},
        }],
        ['OS=="linux" or OS=="mac" or OS=="win"', {
          'variables': {'p': 'q'},
        }],
        ['OS=="mac"', {
          'variables': {'foo': 'bar'},
        }],
      ],
    }
    self.assertEqual(
        expected_output,
        isolate_format.convert_old_to_new_format(isolate_with_else_clauses))

  def test_convert_old_to_new_default_variables(self):
    isolate_with_default_variables = {
      'conditions': [
        ['OS=="abc"', {
          'variables': {'foo': 'bar'},
        }],
      ],
      'variables': {'p': 'q'},
    }
    expected_output = {
      'conditions': [
        ['OS=="abc"', {
          'variables': {'foo': 'bar'},
        }],
        ['OS=="abc" or OS=="linux" or OS=="mac" or OS=="win"', {
          'variables': {'p': 'q'},
        }],
      ],
    }
    self.assertEqual(
        expected_output,
        isolate_format.convert_old_to_new_format(
            isolate_with_default_variables))

  def test_match_configs(self):
    expectations = [
        (
          ('OS=="win"', ('OS',), [('win',), ('mac',), ('linux',)]),
          [('win',)],
        ),
        (
          (
            '(foo==1 or foo==2) and bar=="b"',
            ['foo', 'bar'],
            [(1, 'a'), (1, 'b'), (2, 'a'), (2, 'b')],
          ),
          [(1, 'b'), (2, 'b')],
        ),
        (
          (
            'bar=="b"',
            ['foo', 'bar'],
            [(1, 'a'), (1, 'b'), (2, 'a'), (2, 'b')],
          ),
          # TODO(maruel): When a free variable match is found, it should not
          # list all the bounded values in addition. The problem is when an
          # intersection of two different bound variables that are tested singly
          # in two different conditions.
          [(1, 'b'), (2, 'b'), (None, 'b')],
        ),
        (
          (
            'foo==1 or bar=="b"',
            ['foo', 'bar'],
            [(1, 'a'), (1, 'b'), (2, 'a'), (2, 'b')],
          ),
          # TODO(maruel): (None, 'b') would match.
          # It is hard in this case to realize that each of the variables 'foo'
          # and 'bar' can be unbounded in a specific case.
          [(1, 'a'), (1, 'b'), (2, 'b'), (1, None)],
        ),
    ]
    for data, expected in expectations:
      self.assertEqual(expected, isolate_format.match_configs(*data))


class IsolateFormatTmpDirTest(unittest.TestCase):
  def setUp(self):
    super(IsolateFormatTmpDirTest, self).setUp()
    self.tempdir = tempfile.mkdtemp(prefix='isolate_')

  def tearDown(self):
    try:
      run_isolated.rmtree(self.tempdir)
    finally:
      super(IsolateFormatTmpDirTest, self).tearDown()

  def test_load_with_includes(self):
    included_isolate = {
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
            'read_only': 1,
          },
        }, {
          'variables': {
            'isolate_dependency_tracked': [
              'file_non_linux',
            ],
            'read_only': 0,
          },
        }],
      ],
    }
    tools.write_json(
        os.path.join(self.tempdir, 'included.isolate'), included_isolate, True)
    values = {
      'includes': ['included.isolate'],
      'variables': {
        'isolate_dependency_tracked': [
          'file_less_common',
        ],
      },
      'conditions': [
        ['OS=="mac"', {
          'variables': {
            'isolate_dependency_tracked': [
              'file_mac',
            ],
            'read_only': 2,
          },
        }],
      ],
    }
    actual = isolate_format.load_isolate_as_config(self.tempdir, values, None)

    expected = {
      ('linux',): {
        'isolate_dependency_tracked': [
          'file_common',
          'file_less_common',
          'file_linux',
        ],
        'read_only': 1,
      },
      ('mac',): {
        'isolate_dependency_tracked': [
          'file_common',
          'file_less_common',
          'file_mac',
          'file_non_linux',
        ],
        'read_only': 2,
      },
      ('win',): {
        'isolate_dependency_tracked': [
          'file_common',
          'file_less_common',
          'file_non_linux',
        ],
        'read_only': 0,
      },
    }
    self.assertEqual(expected, actual.flatten())

  def test_load_with_includes_with_commands(self):
    # This one is messy.
    isolate1 = {
      'conditions': [
        ['OS=="linux"', {
          'variables': {
            'command': [
              'foo', 'bar',
            ],
            'isolate_dependency_tracked': [
              'file_linux',
            ],
          },
        }, {
          'variables': {
            'isolate_dependency_tracked': [
              'file_non_linux',
            ],
          },
        }],
        ['OS=="win"', {
          'variables': {
            'command': [
              'foo', 'bar',
            ],
          },
        }],
      ],
    }
    tools.write_json(
        os.path.join(self.tempdir, 'isolate1.isolate'), isolate1, True)
    isolate2 = {
      'conditions': [
        ['OS=="linux" or OS=="mac"', {
          'variables': {
            'command': [
              'zoo',
            ],
            'isolate_dependency_tracked': [
              'other/file',
            ],
          },
        }],
      ],
    }
    tools.write_json(
        os.path.join(self.tempdir, 'isolate2.isolate'), isolate2, True)
    isolate3 = {
      'includes': ['isolate1.isolate', 'isolate2.isolate'],
      'conditions': [
        ['OS=="mac"', {
          'variables': {
            'command': [
              'yo', 'dawg',
            ],
            'isolate_dependency_tracked': [
              'file_mac',
            ],
          },
        }],
      ],
    }

    actual = isolate_format.load_isolate_as_config(self.tempdir, isolate3, None)
    expected = {
      ('linux',): {
        # Last included takes precedence.
        'command': ['zoo'],
        'isolate_dependency_tracked': ['file_linux', 'other/file'],
      },
      ('mac',): {
        # Command in isolate3 takes precedence.
        'command': ['yo', 'dawg'],
        'isolate_dependency_tracked': [
          'file_mac',
          'file_non_linux',
          'other/file',
        ],
      },
      ('win',): {
        'command': ['foo', 'bar'],
        'isolate_dependency_tracked': ['file_non_linux'],
      },
    }
    self.assertEqual(expected, actual.flatten())


if __name__ == '__main__':
  logging.basicConfig(
      level=logging.DEBUG if '-v' in sys.argv else logging.ERROR,
      format='%(levelname)5s %(filename)15s(%(lineno)3d): %(message)s')
  if '-v' in sys.argv:
    unittest.TestCase.maxDiff = None
  unittest.main()
