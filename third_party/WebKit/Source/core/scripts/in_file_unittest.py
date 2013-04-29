#!/usr/bin/env python

import unittest

from in_file import InFile

class InFileTest(unittest.TestCase):
    def test_basic_parse(self):
        contents = """
name1 arg=value, arg2=value2, arg2=value3
name2
"""
        lines = contents.split("\n")
        defaults = {
            'arg': None,
            'arg2': [],
        }
        in_file = InFile(lines, defaults)
        expected_values = [
            {'name': 'name1', 'arg': 'value', 'arg2': ['value2', 'value3']},
            {'name': 'name2', 'arg': None, 'arg2': []},
        ]
        self.assertEquals(in_file.name_dictionaries, expected_values)

if __name__ == "__main__":
    unittest.main()
