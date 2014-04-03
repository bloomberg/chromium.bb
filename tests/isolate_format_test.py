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

import isolate_format
import run_isolated
# Create shortcuts.
from isolate_format import KEY_TOUCHED, KEY_TRACKED, KEY_UNTRACKED


# Access to a protected member XXX of a client class
# pylint: disable=W0212


FAKE_DIR = (
    u'z:\\path\\to\\non_existing'
    if sys.platform == 'win32' else u'/path/to/non_existing')


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

  def test_eval_content(self):
    try:
      # Intrinsics are not available.
      isolate_format.eval_content('map(str, [1, 2])')
      self.fail()
    except NameError:
      pass

  def test_load_isolate_as_config_empty(self):
    expected = {
      (): {
        'isolate_dir': FAKE_DIR,
      },
    }
    self.assertEqual(
        expected,
        isolate_format.load_isolate_as_config(FAKE_DIR, {}, None).flatten())

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
      (None,): {
        'isolate_dir': FAKE_DIR,
      },
      ('amiga',): {
        KEY_TOUCHED: ['touched', 'touched_e'],
        KEY_TRACKED: ['a', 'e', 'g', 'x'],
        KEY_UNTRACKED: ['b', 'f', 'h'],
        'command': ['echo', 'You should get an Atari'],
        'isolate_dir': FAKE_DIR,
        'read_only': 1,
      },
      ('atari',): {
        KEY_TOUCHED: ['touched', 'touched_a'],
        KEY_TRACKED: ['a', 'c', 'x'],
        KEY_UNTRACKED: ['b', 'd', 'h'],
        'command': ['echo', 'Hello World'],
        'isolate_dir': FAKE_DIR,
        'read_only': 2,
      },
      ('coleco',): {
        KEY_TOUCHED: ['touched', 'touched_e'],
        KEY_TRACKED: ['a', 'e', 'x'],
        KEY_UNTRACKED: ['b', 'f'],
        'command': ['echo', 'You should get an Atari'],
        'isolate_dir': FAKE_DIR,
      },
      ('dendy',): {
        KEY_TOUCHED: ['touched', 'touched_e'],
        KEY_TRACKED: ['a', 'e', 'x'],
        KEY_UNTRACKED: ['b', 'f', 'h'],
        'command': ['echo', 'You should get an Atari'],
        'isolate_dir': FAKE_DIR,
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

  def test_load_isolate_as_config_no_variable(self):
    value = {
      'variables': {
        'command': ['echo', 'You should get an Atari'],
        KEY_TRACKED: ['a'],
        KEY_UNTRACKED: ['b'],
        KEY_TOUCHED: ['touched'],
        'read_only': 1,
      },
    }
    # The key is the empty tuple, since there is no variable to bind to.
    expected = {
      (): {
        KEY_TRACKED: ['a'],
        KEY_UNTRACKED: ['b'],
        KEY_TOUCHED: ['touched'],
        'command': ['echo', 'You should get an Atari'],
        'isolate_dir': FAKE_DIR,
        'read_only': 1,
      },
    }
    self.assertEqual(
        expected, isolate_format.load_isolate_as_config(
            FAKE_DIR, value, None).flatten())

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
    actual = isolate_format.Configs(None, ()).union(
        isolate_format.load_isolate_as_config(FAKE_DIR, {}, None)).union(
            isolate_format.load_isolate_as_config(FAKE_DIR, {}, None))
    expected = {
      (): {
        'isolate_dir': FAKE_DIR,
      },
    }
    self.assertEqual(expected, actual.flatten())

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
      (None,): {
        'isolate_dir': FAKE_DIR,
      },
      ('linux',): {
        'isolate_dependency_tracked': ['file_common', 'file_linux'],
        'isolate_dir': FAKE_DIR,
      },
      ('mac',): {
        'isolate_dependency_tracked': ['file_common', 'file_mac'],
        'isolate_dir': FAKE_DIR,
      },
    }
    # Pylint is confused about union() return type.
    # pylint: disable=E1103
    configs = isolate_format.Configs(None, ()).union(
        isolate_format.load_isolate_as_config(FAKE_DIR, linux, None)).union(
            isolate_format.load_isolate_as_config(FAKE_DIR, mac, None)
        ).flatten()
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
      (None, None): {
        'isolate_dir': FAKE_DIR,
      },
      ('linux', 1): {
        'isolate_dependency_tracked': ['file_common', 'file_linux'],
        'isolate_dir': FAKE_DIR,
      },
      ('mac', 0): {
        'isolate_dependency_tracked': ['file_common', 'file_mac'],
        'isolate_dir': FAKE_DIR,
      },
      ('win', 0): {
        'isolate_dependency_tracked': ['file_common', 'file_win'],
        'isolate_dir': FAKE_DIR,
      },
    }
    # Pylint is confused about union() return type.
    # pylint: disable=E1103
    configs = isolate_format.Configs(None, ()).union(
        isolate_format.load_isolate_as_config(FAKE_DIR, linux, None)).union(
            isolate_format.load_isolate_as_config(FAKE_DIR, mac, None)).union(
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
      (None, None): {
        'isolate_dir': FAKE_DIR,
      },
      (None, 'abc'): {
        'command': ['bar'],
        'isolate_dir': FAKE_DIR,
      },
      ('1', None): {
        'command': ['foo'],
        'isolate_dir': FAKE_DIR,
      },
      # TODO(maruel): It is a conflict.
      ('1', 'abc'): {
        'command': ['bar'],
        'isolate_dir': FAKE_DIR,
      },
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
      (None, None): {
        'isolate_dir': FAKE_DIR,
      },
      (None, 'abc'): {
        'command': ['bar'],
        'isolate_dir': FAKE_DIR,
      },
      ('1', None): {
        'command': ['foo'],
        'isolate_dir': FAKE_DIR,
      },
    }
    self.assertEqual(expected, flatten)

  def test_make_isolate_multi_variables(self):
    config = isolate_format.Configs(None, ('CHROMEOS', 'OS'))
    config._by_config[(('0', 'linux'))] = isolate_format.ConfigSettings(
        {'command': ['bar']}, FAKE_DIR)
    config._by_config[(('1', 'linux'))] = isolate_format.ConfigSettings(
        {'command': ['foo']}, FAKE_DIR)
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
        {'command': ['bar']}, FAKE_DIR)
    config._by_config[(('1', None))] = isolate_format.ConfigSettings(
        {'command': ['foo']}, FAKE_DIR)
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
    config = isolate_format.Configs(None, ('BRAND', 'CHROMEOS', 'LIB', 'OS'))
    config._by_config = {
        (None, 0, 's', 'linux'): isolate_format.ConfigSettings(
            {'command': ['bar']}, FAKE_DIR),
        (None, None, 's', 'mac'): isolate_format.ConfigSettings(
            {'command': ['foo']}, FAKE_DIR),
        (None, None, 's', 'win'): isolate_format.ConfigSettings(
            {'command': ['ziz']}, FAKE_DIR),
        ('Chrome', 0, 's', 'win'): isolate_format.ConfigSettings(
            {'command': ['baz']}, FAKE_DIR),
    }
    expected = {
      'conditions': [
        ['BRAND=="Chrome" and CHROMEOS==0 and LIB=="s" and OS=="win"', {
          'variables': {
            'command': ['baz'],
          },
        }],
        ['CHROMEOS==0 and LIB=="s" and OS=="linux"', {
          'variables': {
            'command': ['bar'],
          },
        }],
        ['LIB=="s" and OS=="mac"', {
          'variables': {
            'command': ['foo'],
          },
        }],
        ['LIB=="s" and OS=="win"', {
          'variables': {
            'command': ['ziz'],
          },
        }],
      ],
    }
    self.assertEqual(expected, config.make_isolate_file())

  def test_ConfigSettings_union(self):
    lhs_values = {}
    rhs_values = {KEY_UNTRACKED: ['data/', 'test/data/']}
    lhs = isolate_format.ConfigSettings(lhs_values, '/src/net/third_party/nss')
    rhs = isolate_format.ConfigSettings(rhs_values, '/src/base')
    out = lhs.union(rhs)
    expected = {
      KEY_UNTRACKED: ['data/', 'test/data/'],
      'isolate_dir': '/src/base',
    }
    self.assertEqual(expected, out.flatten())

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
    configs = isolate_format.load_isolate_as_config(
            FAKE_DIR, {}, '# Yo dawg!\n# Chill out.\n').union(
        isolate_format.load_isolate_as_config(FAKE_DIR, {}, None))
    self.assertEqual('# Yo dawg!\n# Chill out.\n', configs.file_comment)

    configs = isolate_format.load_isolate_as_config(FAKE_DIR, {}, None).union(
        isolate_format.load_isolate_as_config(
            FAKE_DIR, {}, '# Yo dawg!\n# Chill out.\n'))
    self.assertEqual('# Yo dawg!\n# Chill out.\n', configs.file_comment)

    # Only keep the first one.
    configs = isolate_format.load_isolate_as_config(
        FAKE_DIR, {}, '# Yo dawg!\n').union(
            isolate_format.load_isolate_as_config(
                FAKE_DIR, {}, '# Chill out.\n'))
    self.assertEqual('# Yo dawg!\n', configs.file_comment)

  def test_extract_comment(self):
    self.assertEqual(
        '# Foo\n# Bar\n', isolate_format.extract_comment('# Foo\n# Bar\n{}'))
    self.assertEqual('', isolate_format.extract_comment('{}'))

  def _test_pretty_print_impl(self, value, expected):
    actual = cStringIO.StringIO()
    isolate_format.pretty_print(value, actual)
    self.assertEqual(expected.splitlines(), actual.getvalue().splitlines())

  def test_pretty_print_empty(self):
    self._test_pretty_print_impl({}, '{\n}\n')

  def test_pretty_print_mid_size(self):
    value = {
      'variables': {
        KEY_TOUCHED: [
          'file1',
          'file2',
        ],
      },
      'conditions': [
        ['OS==\"foo\"', {
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
          },
        }],
        ['OS==\"bar\"', {
          'variables': {},
        }],
      ],
    }
    isolate_format.verify_root(value, {})
    # This is an .isolate format.
    expected = (
        "{\n"
        "  'variables': {\n"
        "    'isolate_dependency_touched': [\n"
        "      'file1',\n"
        "      'file2',\n"
        "    ],\n"
        "  },\n"
        "  'conditions': [\n"
        "    ['OS==\"foo\"', {\n"
        "      'variables': {\n"
        "        'command': [\n"
        "          'python',\n"
        "          '-c',\n"
        "          'print \"H\\i\'\"',\n"
        "        ],\n"
        "        'read_only': 2,\n"
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
        "    ['OS==\"bar\"', {\n"
        "      'variables': {\n"
        "      },\n"
        "    }],\n"
        "  ],\n"
        "}\n")
    self._test_pretty_print_impl(value, expected)

  def test_convert_old_to_new_else(self):
    isolate_with_else_clauses = {
      'conditions': [
        ['OS=="mac"', {
          'variables': {'foo': 'bar'},
        }, {
          'variables': {'x': 'y'},
        }],
      ],
    }
    with self.assertRaises(isolate_format.isolateserver.ConfigError):
      isolate_format.load_isolate_as_config(
          FAKE_DIR, isolate_with_else_clauses, None)

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

  def test_load_with_globals(self):
    values = {
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
        }],
        ['OS=="mac" or OS=="win"', {
          'variables': {
            'isolate_dependency_tracked': [
              'file_non_linux',
            ],
            'read_only': 0,
          },
        }],
      ],
    }
    expected = {
      (None,): {
        'isolate_dependency_tracked': [
          'file_common',
        ],
        'isolate_dir': FAKE_DIR,
      },
      ('linux',): {
        'isolate_dependency_tracked': [
          'file_linux',
        ],
        'isolate_dir': FAKE_DIR,
        'read_only': 1,
      },
      ('mac',): {
        'isolate_dependency_tracked': [
          'file_non_linux',
        ],
        'isolate_dir': FAKE_DIR,
        'read_only': 0,
      },
      ('win',): {
        'isolate_dependency_tracked': [
          'file_non_linux',
        ],
        'isolate_dir': FAKE_DIR,
        'read_only': 0,
      },
    }
    actual = isolate_format.load_isolate_as_config(FAKE_DIR, values, None)
    self.assertEqual(expected, actual.flatten())

  def test_configs_with_globals(self):
    c = isolate_format.Configs(None, ('x', 'y'))
    c.set_config(
        (1, 1), isolate_format.ConfigSettings({KEY_TRACKED: ['1,1']}, FAKE_DIR))
    c.set_config(
        (2, 2), isolate_format.ConfigSettings({KEY_TRACKED: ['2,2']}, FAKE_DIR))
    c.set_config(
        (1, None),
        isolate_format.ConfigSettings({KEY_TRACKED: ['1,y']}, FAKE_DIR))
    c.set_config(
        (None, 2),
        isolate_format.ConfigSettings({KEY_TRACKED: ['x,2']}, FAKE_DIR))
    c.set_config(
        (None, None),
        isolate_format.ConfigSettings({KEY_TRACKED: ['x,y']}, FAKE_DIR))
    expected = {
      (None, None): {
        KEY_TRACKED: ['x,y'],
        'isolate_dir': FAKE_DIR,
      },
      (None, 2): {
        KEY_TRACKED: ['x,2'],
        'isolate_dir': FAKE_DIR,
      },
      (1, None): {
        KEY_TRACKED: ['1,y'],
        'isolate_dir': FAKE_DIR,
      },
      (1, 1): {
        KEY_TRACKED: ['1,1'],
        'isolate_dir': FAKE_DIR,
      },
      (2, 2): {
        KEY_TRACKED: ['2,2'],
        'isolate_dir': FAKE_DIR,
      },
    }
    self.assertEqual(expected, c.flatten())

    s = c.get_config((1, 1))
    expected = {
      KEY_TRACKED: ['1,1', '1,y', 'x,y'],
      'isolate_dir': FAKE_DIR,
    }
    self.assertEqual(expected, s.flatten())

    s = c.get_config((1, None))
    expected = {
      KEY_TRACKED: ['1,y', 'x,y'],
      'isolate_dir': FAKE_DIR,
    }
    self.assertEqual(expected, s.flatten())

    s = c.get_config((None, None))
    expected = {
      KEY_TRACKED: ['x,y'],
      'isolate_dir': FAKE_DIR,
    }
    self.assertEqual(expected, s.flatten())

    expected = {
      'conditions': [
        ['x==1', {'variables': {KEY_TRACKED: ['1,y']}}],
        ['x==1 and y==1', {'variables': {KEY_TRACKED: ['1,1']}}],
        ['x==2 and y==2', {'variables': {KEY_TRACKED: ['2,2']}}],
        ['y==2', {'variables': {KEY_TRACKED: ['x,2']}}],
      ],
      'variables': {KEY_TRACKED: ['x,y']},
    }
    self.assertEqual(expected, c.make_isolate_file())


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
        }],
        ['OS=="mac" or OS=="win"', {
          'variables': {
            'isolate_dependency_tracked': [
              'file_non_linux',
            ],
            'read_only': 0,
          },
        }],
      ],
    }
    with open(os.path.join(self.tempdir, 'included.isolate'), 'wb') as f:
      isolate_format.pretty_print(included_isolate, f)
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
      (None,): {
        'isolate_dependency_tracked': [
          'file_common',
          'file_less_common',
        ],
        'isolate_dir': self.tempdir,
      },
      ('linux',): {
        'isolate_dependency_tracked': [
          'file_linux',
        ],
        'isolate_dir': self.tempdir,
        'read_only': 1,
      },
      ('mac',): {
        'isolate_dependency_tracked': [
          'file_mac',
          'file_non_linux',
        ],
        'isolate_dir': self.tempdir,
        'read_only': 2,
      },
      ('win',): {
        'isolate_dependency_tracked': [
          'file_non_linux',
        ],
        'isolate_dir': self.tempdir,
        'read_only': 0,
      },
    }
    self.assertEqual(expected, actual.flatten())

  def test_load_with_includes_with_commands(self):
    # This one is messy. Check that isolate_dir is the expected value. To
    # achieve this, put the .isolate files into subdirectories.
    dir_1 = os.path.join(self.tempdir, '1')
    dir_3 = os.path.join(self.tempdir, '3')
    dir_3_2 = os.path.join(self.tempdir, '3', '2')
    os.mkdir(dir_1)
    os.mkdir(dir_3)
    os.mkdir(dir_3_2)

    isolate1 = {
      'conditions': [
        ['OS=="amiga" or OS=="win"', {
          'variables': {
            'command': [
              'foo', 'amiga_or_win',
            ],
          },
        }],
        ['OS=="linux"', {
          'variables': {
            'command': [
              'foo', 'linux',
            ],
            'isolate_dependency_tracked': [
              'file_linux',
            ],
          },
        }],
        ['OS=="mac" or OS=="win"', {
          'variables': {
            'isolate_dependency_tracked': [
              'file_non_linux',
            ],
          },
        }],
      ],
    }
    isolate2 = {
      'conditions': [
        ['OS=="linux" or OS=="mac"', {
          'variables': {
            'command': [
              'foo', 'linux_or_mac',
            ],
            'isolate_dependency_tracked': [
              'other/file',
            ],
          },
        }],
      ],
    }
    isolate3 = {
      'includes': [
        '../1/isolate1.isolate',
        '2/isolate2.isolate',
      ],
      'conditions': [
        ['OS=="amiga"', {
          'variables': {
            'isolate_dependency_tracked': [
              'file_amiga',
            ],
          },
        }],
        ['OS=="mac"', {
          'variables': {
            'command': [
              'foo', 'mac',
            ],
            'isolate_dependency_tracked': [
              'file_mac',
            ],
          },
        }],
      ],
    }
    # No need to write isolate3.
    with open(os.path.join(dir_1, 'isolate1.isolate'), 'wb') as f:
      isolate_format.pretty_print(isolate1, f)
    with open(os.path.join(dir_3_2, 'isolate2.isolate'), 'wb') as f:
      isolate_format.pretty_print(isolate2, f)

    # The 'isolate_dir' are important, they are what will be used when
    # definining the final isolate_dir to use to run the command in the
    # .isolated file.
    actual = isolate_format.load_isolate_as_config(dir_3, isolate3, None)
    expected = {
      (None,): {
        # TODO(maruel): See TODO in ConfigSettings.flatten().
        # TODO(maruel): If kept, in this case dir_3 should be selected.
        'isolate_dir': dir_1,
      },
      ('amiga',): {
        'command': ['foo', 'amiga_or_win'],
        'isolate_dependency_tracked': [
          # Note that the file was rebased from isolate1. This is important,
          # isolate1 represent the canonical root path because it is the one
          # that defined the command.
          '../3/file_amiga',
        ],
        'isolate_dir': dir_1,
      },
      ('linux',): {
        # Last included takes precedence. *command comes from isolate2*, so
        # it becomes the canonical root, so reference to file from isolate1 is
        # via '../../1'.
        'command': ['foo', 'linux_or_mac'],
        'isolate_dependency_tracked': [
          '../../1/file_linux',
          'other/file',
        ],
        'isolate_dir': dir_3_2,
      },
      ('mac',): {
        # command in isolate3 takes precedence over the ones included.
        'command': ['foo', 'mac'],
        'isolate_dependency_tracked': [
          '../1/file_non_linux',
          '2/other/file',
          'file_mac',
        ],
        'isolate_dir': dir_3,
      },
      ('win',): {
        # command comes from isolate1.
        'command': ['foo', 'amiga_or_win'],
        'isolate_dependency_tracked': [
          # While this may be surprising, this is because the command was
          # defined in isolate1, not isolate3.
          'file_non_linux',
        ],
        'isolate_dir': dir_1,
      },
    }
    self.assertEqual(expected, actual.flatten())

  def test_load_with_includes_with_commands_and_variables(self):
    # This one is the pinacle of fun. Check that isolate_dir is the expected
    # value. To achieve this, put the .isolate files into subdirectories.
    dir_1 = os.path.join(self.tempdir, '1')
    dir_3 = os.path.join(self.tempdir, '3')
    dir_3_2 = os.path.join(self.tempdir, '3', '2')
    os.mkdir(dir_1)
    os.mkdir(dir_3)
    os.mkdir(dir_3_2)

    isolate1 = {
      'conditions': [
        ['OS=="amiga" or OS=="win"', {
          'variables': {
            'command': [
              'foo', 'amiga_or_win', '<(PATH)',
            ],
          },
        }],
        ['OS=="linux"', {
          'variables': {
            'command': [
              'foo', 'linux', '<(PATH)',
            ],
            'isolate_dependency_tracked': [
              '<(PATH)/file_linux',
            ],
          },
        }],
        ['OS=="mac" or OS=="win"', {
          'variables': {
            'isolate_dependency_tracked': [
              '<(PATH)/file_non_linux',
            ],
          },
        }],
      ],
    }
    isolate2 = {
      'conditions': [
        ['OS=="linux" or OS=="mac"', {
          'variables': {
            'command': [
              'foo', 'linux_or_mac', '<(PATH)',
            ],
            'isolate_dependency_tracked': [
              '<(PATH)/other/file',
            ],
          },
        }],
      ],
    }
    isolate3 = {
      'includes': [
        '../1/isolate1.isolate',
        '2/isolate2.isolate',
      ],
      'conditions': [
        ['OS=="amiga"', {
          'variables': {
            'isolate_dependency_tracked': [
              '<(PATH)/file_amiga',
            ],
          },
        }],
        ['OS=="mac"', {
          'variables': {
            'command': [
              'foo', 'mac', '<(PATH)',
            ],
            'isolate_dependency_tracked': [
              '<(PATH)/file_mac',
            ],
          },
        }],
      ],
    }
    # No need to write isolate3.
    with open(os.path.join(dir_1, 'isolate1.isolate'), 'wb') as f:
      isolate_format.pretty_print(isolate1, f)
    with open(os.path.join(dir_3_2, 'isolate2.isolate'), 'wb') as f:
      isolate_format.pretty_print(isolate2, f)

    # The 'isolate_dir' are important, they are what will be used when
    # definining the final isolate_dir to use to run the command in the
    # .isolated file.
    actual = isolate_format.load_isolate_as_config(dir_3, isolate3, None)
    expected = {
      (None,): {
        'isolate_dir': dir_1,
      },
      ('amiga',): {
        'command': ['foo', 'amiga_or_win', '<(PATH)'],
        'isolate_dependency_tracked': [
          '<(PATH)/file_amiga',
        ],
        'isolate_dir': dir_1,
      },
      ('linux',): {
        # Last included takes precedence. *command comes from isolate2*, so
        # it becomes the canonical root, so reference to file from isolate1 is
        # via '../../1'.
        'command': ['foo', 'linux_or_mac', '<(PATH)'],
        'isolate_dependency_tracked': [
          '<(PATH)/file_linux',
          '<(PATH)/other/file',
        ],
        'isolate_dir': dir_3_2,
      },
      ('mac',): {
        'command': ['foo', 'mac', '<(PATH)'],
        'isolate_dependency_tracked': [
          '<(PATH)/file_mac',
          '<(PATH)/file_non_linux',
          '<(PATH)/other/file',
        ],
        'isolate_dir': dir_3,
      },
      ('win',): {
        # command comes from isolate1.
        'command': ['foo', 'amiga_or_win', '<(PATH)'],
        'isolate_dependency_tracked': [
          '<(PATH)/file_non_linux',
        ],
        'isolate_dir': dir_1,
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
