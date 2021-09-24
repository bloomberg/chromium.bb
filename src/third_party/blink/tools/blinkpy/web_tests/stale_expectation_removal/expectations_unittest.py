#!/usr/bin/env vpython3
# Copyright 2021 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os
import unittest

import six

if six.PY3:
    import unittest.mock as mock

from pyfakefs import fake_filesystem_unittest

from blinkpy.web_tests.stale_expectation_removal import expectations


def CreateFile(fs, *args, **kwargs):
    # TODO(crbug.com/1156806): Remove this and just use fs.create_file() when
    # Catapult is updated to a newer version of pyfakefs that is compatible
    # with Chromium's version.
    if hasattr(fs, 'create_file'):
        fs.create_file(*args, **kwargs)
    else:
        fs.CreateFile(*args, **kwargs)


@unittest.skipIf(six.PY2, 'Script and unittest are Python 3-only')
class GetExpectationFilepathsUnittest(fake_filesystem_unittest.TestCase):
    def setUp(self):
        self.setUpPyfakefs()
        self.instance = expectations.WebTestExpectations()
        CreateFile(
            self.fs,
            os.path.join(expectations.WEB_TEST_ROOT_DIR, 'FlagExpectations',
                         'README.txt'))

    def testRealFilesCanBeFound(self):
        """Tests that real files are returned."""
        with fake_filesystem_unittest.Pause(self):
            filepaths = self.instance.GetExpectationFilepaths()
            self.assertTrue(len(filepaths) > 0)
            for f in filepaths:
                self.assertTrue(os.path.exists(f))

    def testTopLevelFiles(self):
        """Tests that top-level expectation files are properly returned."""
        with mock.patch.object(self.instance,
                               '_GetTopLevelExpectationFiles',
                               return_value=['/foo']):
            filepaths = self.instance.GetExpectationFilepaths()
        self.assertEqual(filepaths, ['/foo'])

    def testFlagSpecificFiles(self):
        """Tests that flag-specific files are properly returned."""
        flag_filepath = os.path.join(expectations.WEB_TEST_ROOT_DIR,
                                     'FlagExpectations', 'foo-flag')
        CreateFile(self.fs, flag_filepath)
        with mock.patch.object(self.instance,
                               '_GetTopLevelExpectationFiles',
                               return_value=[]):
            filepaths = self.instance.GetExpectationFilepaths()
        self.assertEqual(filepaths, [flag_filepath])

    def testAllExpectationFiles(self):
        """Tests that both top level and flag-specific files are returned."""
        flag_filepath = os.path.join(expectations.WEB_TEST_ROOT_DIR,
                                     'FlagExpectations', 'foo-flag')
        CreateFile(self.fs, flag_filepath)
        with mock.patch.object(self.instance,
                               '_GetTopLevelExpectationFiles',
                               return_value=['/foo']):
            filepaths = self.instance.GetExpectationFilepaths()
        self.assertEqual(filepaths, ['/foo', flag_filepath])


@unittest.skipIf(six.PY2, 'Script and unittest are Python 3-only')
class GetExpectationFileTagHeaderUnittest(fake_filesystem_unittest.TestCase):
    def setUp(self):
        self.setUpPyfakefs()
        self.instance = expectations.WebTestExpectations()

    def testRealContentsCanBeLoaded(self):
        """Tests that some sort of valid content can be read from the file."""
        with fake_filesystem_unittest.Pause(self):
            header = self.instance._GetExpectationFileTagHeader()
        self.assertIn('tags', header)
        self.assertIn('results', header)

    def testContentLoading(self):
        """Tests that the header is properly loaded."""
        header_contents = """\

# foo
#   bar

# baz

not a comment
"""
        CreateFile(self.fs, expectations.MAIN_EXPECTATION_FILE)
        with open(expectations.MAIN_EXPECTATION_FILE, 'w') as f:
            f.write(header_contents)
        header = self.instance._GetExpectationFileTagHeader()
        expected_header = """\
# foo
#   bar
# baz
"""
        self.assertEqual(header, expected_header)


if __name__ == '__main__':
    unittest.main(verbosity=2)
