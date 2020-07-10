# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import StringIO
import unittest

from blinkpy.common.system.system_host_mock import MockSystemHost
from blinkpy.formatter.main import main


ACTUAL_INPUT = '''
def foo():
    """triple-quoted docstring"""
    try:
        bar = "bar"
        long_list = ['this is a list of strings that should be wrapped', "and consistently quoted"]
        longer_list = ['this is a list of strings that should be wrapped', "and consistently quoted", "because it's important to test quoting"]
    except Exception, error:
        pass
'''


EXPECTED_BLINK_OUTPUT = '''
def foo():
    """triple-quoted docstring"""
    try:
        bar = 'bar'
        long_list = ['this is a list of strings that should be wrapped', 'and consistently quoted']
        longer_list = [
            'this is a list of strings that should be wrapped',
            'and consistently quoted',
            "because it's important to test quoting"]
    except Exception as error:
        pass
'''


EXPECTED_CHROMIUM_OUTPUT = '''
def foo():
  """triple-quoted docstring"""
  try:
    bar = 'bar'
    long_list = [
        'this is a list of strings that should be wrapped',
        'and consistently quoted']
    longer_list = [
        'this is a list of strings that should be wrapped',
        'and consistently quoted',
        "because it's important to test quoting"]
  except Exception as error:
    pass
'''

EXPECTED_ONLY_DOUBLE_QUOTED_OUTPUT = '''
def foo():
    """triple-quoted docstring"""
    try:
        bar = "bar"
        long_list = ["this is a list of strings that should be wrapped", "and consistently quoted"]
        longer_list = ["this is a list of strings that should be wrapped", "and consistently quoted", "because it's important to test quoting"]
    except Exception, error:
        pass
'''


class TestMain(unittest.TestCase):
    maxDiff = 4096

    def test_files_blink(self):
        host = MockSystemHost()
        host.filesystem.files = {
            'test.py': ACTUAL_INPUT}
        main(host, ['test.py'])
        self.assertEqual(host.filesystem.files, {
            'test.py': EXPECTED_BLINK_OUTPUT,
            'test.py.bak': ACTUAL_INPUT})

    def test_files_blink_no_backup(self):
        host = MockSystemHost()
        host.filesystem.files = {
            'test.py': ACTUAL_INPUT}
        main(host, ['--no-backups', 'test.py'])
        self.assertEqual(host.filesystem.files, {
            'test.py': EXPECTED_BLINK_OUTPUT})

    def test_stdin_blink(self):
        host = MockSystemHost()
        host.stdin = StringIO.StringIO(ACTUAL_INPUT)
        main(host, ['-'])
        self.assertMultiLineEqual(host.stdout.getvalue(), EXPECTED_BLINK_OUTPUT)

    def test_stdin_chromium(self):
        host = MockSystemHost()
        host.stdin = StringIO.StringIO(ACTUAL_INPUT)
        main(host, ['--chromium', '-'])
        self.assertMultiLineEqual(host.stdout.getvalue(), EXPECTED_CHROMIUM_OUTPUT)

    def test_stdin_no_changes(self):
        host = MockSystemHost()
        host.stdin = StringIO.StringIO(ACTUAL_INPUT)
        main(host, ['--no-autopep8', '--leave-strings-alone', '-'])
        self.assertMultiLineEqual(host.stdout.getvalue(), ACTUAL_INPUT)

    def test_stdin_only_double_quoting(self):
        host = MockSystemHost()
        host.stdin = StringIO.StringIO(ACTUAL_INPUT)
        main(host, ['--no-autopep8', '--double-quote-strings', '-'])
        self.assertMultiLineEqual(host.stdout.getvalue(), EXPECTED_ONLY_DOUBLE_QUOTED_OUTPUT)

    def test_format_docstrings(self):
        host = MockSystemHost()
        host.stdin = StringIO.StringIO('''
def f():
    """
    triple-quoted docstring
    with multiple lines

    """
    x = """
    this is a regular multi-line string, not a docstring
    """
    return x
''')
        main(host, ['-'])
        self.assertMultiLineEqual(host.stdout.getvalue(), '''
def f():
    """triple-quoted docstring
    with multiple lines
    """
    x = """
    this is a regular multi-line string, not a docstring
    """
    return x
''')

    def test_format_docstrings_indentation(self):
        host = MockSystemHost()
        host.stdin = StringIO.StringIO('''
def f():
    """This is a docstring
       With extra indentation on this line.

     """
''')
        main(host, ['-'])
        self.assertMultiLineEqual(host.stdout.getvalue(), '''
def f():
    """This is a docstring
       With extra indentation on this line.
    """
''')
