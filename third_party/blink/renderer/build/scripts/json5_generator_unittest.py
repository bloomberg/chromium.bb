# Copyright 2019 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os
import unittest

from json5_generator import Json5File


class Json5FileTest(unittest.TestCase):
    def path_of_test_file(self, file_name):
        return os.path.join(
            os.path.dirname(os.path.realpath(__file__)), 'tests', file_name)

    def test_valid_dict_value_parse(self):
        actual = Json5File.load_from_files([self.path_of_test_file(
            'json5_generator_valid_dict_value.json5')]).name_dictionaries
        expected = [
            {'name': 'item1', 'param1': {'keys': 'valid', 'random': 'values'}},
            {'name': 'item2', 'param1': {'random': 'values', 'default': 'valid'}}
        ]
        self.assertEqual(len(actual), len(expected))
        for exp, act in zip(expected, actual):
            self.assertDictEqual(exp['param1'], act['param1'])

    def test_no_valid_keys(self):
        with self.assertRaises(AssertionError):
            Json5File.load_from_files([self.path_of_test_file('json5_generator_no_valid_keys.json5')])

    def test_value_not_in_valid_values(self):
        with self.assertRaises(Exception):
            Json5File.load_from_files([self.path_of_test_file('json5_generator_invalid_value.json5')])

    def test_key_not_in_valid_keys(self):
        with self.assertRaises(Exception):
            Json5File.load_from_files([self.path_of_test_file('json5_generator_invalid_key.json5')])

if __name__ == "__main__":
    unittest.main()
