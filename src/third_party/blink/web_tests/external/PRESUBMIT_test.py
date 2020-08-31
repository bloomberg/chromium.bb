#!/usr/bin/env python
# Copyright (c) 2018 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os
import subprocess
import sys
import unittest

import PRESUBMIT


class MockInputApi(object):
    """A minimal mock InputApi for our checks."""

    def __init__(self):
        self.affected_paths = []
        self.os_path = os.path
        self.python_executable = sys.executable
        self.subprocess = subprocess

    def AbsoluteLocalPaths(self):
        return self.affected_paths

    def PresubmitLocalPath(self):
        return os.path.abspath(os.path.dirname(__file__))

    def AffectedSourceFiles(self, filter_func):
        all_files = [MockFile(self.PresubmitLocalPath(), path) for path in self.affected_paths]
        return filter(lambda f: filter_func(f), all_files)


class MockPresubmitError(object):
    """A minimal mock of an error class for our checks."""

    def __init__(self, message, items, long_text):
        self.message = message
        self.items = items
        self.long_text = long_text


class MockPresubmitWarning(object):
    """A minimal mock of an warning class for our checks."""

    def __init__(self, message, items, long_text):
        self.message = message
        self.items = items
        self.long_text = long_text


class MockOutputApi(object):
    """A minimal mock OutputApi for our checks."""

    def PresubmitError(self, message, items=None, long_text=''):
        return MockPresubmitError(message, items, long_text)

    def PresubmitPromptWarning(self, message, items=None, long_text=''):
        return MockPresubmitWarning(message, items, long_text)


class MockFile(object):
    """A minimal mock File for our checks."""

    def __init__(self, presubmit_abs_path, file_abs_path):
        self.abs_path = file_abs_path
        self.local_path = os.path.relpath(file_abs_path, presubmit_abs_path)

    def AbsoluteLocalPath(self):
        return self.abs_path

    def LocalPath(self):
        return self.local_path


class LintWPTTest(unittest.TestCase):
    def setUp(self):
        self._test_file = os.path.join(os.path.dirname(__file__), 'wpt', '_DO_NOT_SUBMIT_.html')

    def tearDown(self):
        os.remove(self._test_file)

    def testWPTLintSuccess(self):
        with open(self._test_file, 'w') as f:
            f.write('<body>Hello, world!</body>')
        mock_input = MockInputApi()
        mock_output = MockOutputApi()
        mock_input.affected_paths = [os.path.abspath(self._test_file)]
        errors = PRESUBMIT._LintWPT(mock_input, mock_output)
        self.assertEqual(len(errors), 0)

    def testWPTLintErrors(self):
        # Private LayoutTests APIs are not allowed.
        with open(self._test_file, 'w') as f:
            f.write('<script>testRunner.notifyDone()</script>')
        mock_input = MockInputApi()
        mock_output = MockOutputApi()
        mock_input.affected_paths = [os.path.abspath(self._test_file)]
        errors = PRESUBMIT._LintWPT(mock_input, mock_output)
        self.assertEqual(len(errors), 1)


class DontModifyIDLFilesTest(unittest.TestCase):
    def testModifiesIDL(self):
        mock_input = MockInputApi()
        mock_output = MockOutputApi()
        mock_input.affected_paths = [os.path.join(mock_input.PresubmitLocalPath(), 'wpt', 'interfaces', 'test.idl')]
        errors = PRESUBMIT._DontModifyIDLFiles(mock_input, mock_output)
        self.assertEqual(len(errors), 1)
        self.assertTrue(isinstance(errors[0], MockPresubmitWarning))

    def testModifiesNonIDLFiles(self):
        mock_input = MockInputApi()
        mock_output = MockOutputApi()
        mock_input.affected_paths = [os.path.join(mock_input.PresubmitLocalPath(), 'wpt', 'css', 'foo.html')]
        errors = PRESUBMIT._DontModifyIDLFiles(mock_input, mock_output)
        self.assertEqual(len(errors), 0)

    def testModifiesInterfaceDirOutsideOfWPT(self):
        mock_input = MockInputApi()
        mock_output = MockOutputApi()
        mock_input.affected_paths = [os.path.join(mock_input.PresubmitLocalPath(), 'other', 'interfaces', 'test.idl')]
        errors = PRESUBMIT._DontModifyIDLFiles(mock_input, mock_output)
        self.assertEqual(len(errors), 0)


if __name__ == '__main__':
    unittest.main()
