#!/usr/bin/env python
# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import unittest

import merge_gyp


class MergeGyp(unittest.TestCase):
  def test_unknown_key(self):
    try:
      merge_gyp.process_variables(None, {'foo': [],})
      self.fail()
    except AssertionError:
      pass

  def test_unknown_var(self):
    try:
      merge_gyp.process_variables(None, {'variables': {'foo': [],}})
      self.fail()
    except AssertionError:
      pass

  def test_parse_gyp_dict_empty(self):
    f, d, o = merge_gyp.parse_gyp_dict({})
    self.assertEquals({}, f)
    self.assertEquals({}, d)
    self.assertEquals(set(), o)

  def test_parse_gyp_dict(self):
    value = {
      'variables': {
        'isolate_files': [
          'a',
        ],
        'isolate_dirs': [
          'b',
        ],
      },
      'conditions': [
        ['OS=="atari"', {
          'variables': {
            'isolate_files': [
              'c',
              'x',
            ],
            'isolate_dirs': [
              'd',
            ],
          },
        }, {  # else
          'variables': {
            'isolate_files': [
              'e',
              'x',
            ],
            'isolate_dirs': [
              'f',
            ],
          },
        }],
        ['OS=="amiga"', {
          'variables': {
            'isolate_files': [
              'g',
            ],
          },
        }],
        ['OS=="inexistent"', {
        }],
        ['OS=="coleco"', {
        }, {  # else
          'variables': {
            'isolate_dirs': [
              'h',
            ],
          },
        }],
      ],
    }
    expected_files = {
      'a': set([None]),
      'c': set(['atari']),
      'e': set(['!atari']),
      'g': set(['amiga']),
      'x': set(['!atari', 'atari']),  # potential for reduction
    }
    expected_dirs = {
      'b': set([None]),
      'd': set(['atari']),
      'f': set(['!atari']),
      'h': set(['!coleco']),
    }
    # coleco is included even if only negative.
    expected_oses = set(['atari', 'amiga', 'coleco'])
    actual_files, actual_dirs, actual_oses = merge_gyp.parse_gyp_dict(value)
    self.assertEquals(expected_files, actual_files)
    self.assertEquals(expected_dirs, actual_dirs)
    self.assertEquals(expected_oses, actual_oses)

  def test_reduce_inputs(self):
    value_files = {
      'a': set([None]),
      'c': set(['atari']),
      'e': set(['!atari']),
      'g': set(['amiga']),
      'x': set(['!atari', 'atari']),
    }
    value_dirs = {
      'b': set([None]),
      'd': set(['atari']),
      'f': set(['!atari']),
      'h': set(['!coleco']),
    }
    value_oses = set(['atari', 'amiga', 'coleco'])
    expected_files = {
      'a': set([None]),
      'c': set(['atari']),
      'e': set(['!atari']),
      'g': set(['amiga']),
      'x': set([None]),  # Reduced.
    }
    expected_dirs = {
      'b': set([None]),
      'd': set(['atari']),
      'f': set(['!atari']),
      'h': set(['!coleco']),
    }
    actual_files, actual_dirs = merge_gyp.reduce_inputs(
        value_files, value_dirs, value_oses)
    self.assertEquals(expected_files, actual_files)
    self.assertEquals(expected_dirs, actual_dirs)

  def test_convert_to_gyp(self):
    files = {
      'a': set([None]),
      'x': set([None]),

      'g': set(['amiga']),

      'c': set(['atari']),
      'e': set(['!atari']),
    }
    dirs = {
      'b': set([None]),

      'd': set(['atari']),
      'f': set(['!atari']),

      'h': set(['!coleco']),
    }
    expected = {
      'variables': {
        'isolate_dirs': ['b'],
        'isolate_files': ['a', 'x'],
      },
      'conditions': [
        ['OS=="amiga"', {
          'variables': {
            'isolate_files': ['g'],
          },
        }],
        ['OS=="atari"', {
          'variables': {
            'isolate_dirs': ['d'],
            'isolate_files': ['c'],
          },
        }, {
          'variables': {
            'isolate_dirs': ['f'],
            'isolate_files': ['e'],
          },
        }],
        ['OS=="coleco"', {
        }, {
          'variables': {
            'isolate_dirs': ['h'],
          },
        }],
      ],
    }
    self.assertEquals(expected, merge_gyp.convert_to_gyp(files, dirs))


if __name__ == '__main__':
  unittest.main()
