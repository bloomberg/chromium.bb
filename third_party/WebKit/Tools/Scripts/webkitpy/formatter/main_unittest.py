# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import StringIO
import unittest

from webkitpy.common.system.systemhost_mock import MockSystemHost
from webkitpy.formatter.main import main


ACTUAL_INPUT = '''
def foo():
    """triple-quoted docstring"""
    try:
        bar = "bar"
        long_list = ['this is a list of strings that should be wrapped', "and consistently quoted"]
        longer_list = ['this is a list of strings that should be wrapped', "and consistently quoted", "because it's important to test quoting"]
    except Exception, e:
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
    except Exception as e:
        pass
'''


EXPECTED_CHROMIUM_OUTPUT = '''
def foo():
  """triple-quoted docstring"""
  try:
    bar = "bar"
    long_list = [
        "this is a list of strings that should be wrapped",
        "and consistently quoted"]
    longer_list = [
        "this is a list of strings that should be wrapped",
        "and consistently quoted",
        "because it's important to test quoting"]
  except Exception as e:
    pass
'''


class TestMain(unittest.TestCase):
    maxDiff = 4096

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
