#!/usr/bin/env vpython3
# Copyright 2021 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import base64
import os
import sys
import unittest

# This script is not Python 2-compatible, but some presubmit scripts end up
# trying to parse this to find tests.
# TODO(crbug.com/1198237): Remove this once all the GPU tests, and by
# extension the presubmit scripts, are Python 3-compatible.
if sys.version_info[0] == 3:
  import unittest.mock as mock

import validate_tag_consistency

from pyfakefs import fake_filesystem_unittest

from flake_suppressor import expectations
from flake_suppressor import unittest_utils as uu


# Note for all tests in this class: We can safely check the contents of the file
# at the end despite potentially having multiple added lines because Python 3.7+
# guarantees that dictionaries remember insertion order, so there is no risk of
# the order of modification changing.
@unittest.skipIf(sys.version_info[0] != 3, 'Python 3-only')
class IterateThroughResultsForUserUnittest(fake_filesystem_unittest.TestCase):
  def setUp(self):
    self._new_stdout = open(os.devnull, 'w')
    self.setUpPyfakefs()
    # Redirect stdout since the tested function prints a lot.
    self._old_stdout = sys.stdout
    sys.stdout = self._new_stdout

    self._input_patcher = mock.patch.object(expectations,
                                            'PromptUserForExpectationAction')
    self._input_mock = self._input_patcher.start()
    self.addCleanup(self._input_patcher.stop)

    self.result_map = {
        'pixel_integration_test': {
            'foo_test': {
                tuple(['win']): ['a'],
                tuple(['mac']): ['b'],
            },
            'bar_test': {
                tuple(['win']): ['c'],
            },
        },
    }

    self.expectation_file = os.path.join(
        expectations.ABSOLUTE_EXPECTATION_FILE_DIRECTORY,
        'pixel_expectations.txt')
    uu.CreateFile(self, self.expectation_file)
    expectation_file_contents = validate_tag_consistency.TAG_HEADER + """\
[ win ] some_test [ Failure ]
[ mac ] some_test [ Failure ]
[ android ] some_test [ Failure ]
"""
    with open(self.expectation_file, 'w') as outfile:
      outfile.write(expectation_file_contents)

  def tearDown(self):
    sys.stdout = self._old_stdout
    self._new_stdout.close()

  def testIterateThroughResultsForUserIgnoreNoGroupByTags(self):
    """Tests that everything appears to function with ignore and no group."""
    self._input_mock.return_value = (None, None)
    expectations.IterateThroughResultsForUser(self.result_map, False)
    expected_contents = validate_tag_consistency.TAG_HEADER + """\
[ win ] some_test [ Failure ]
[ mac ] some_test [ Failure ]
[ android ] some_test [ Failure ]
"""
    with open(self.expectation_file) as infile:
      self.assertEqual(infile.read(), expected_contents)

  def testIterateThroughResultsForUserIgnoreGroupByTags(self):
    """Tests that everything appears to function with ignore and grouping."""
    self._input_mock.return_value = (None, None)
    expectations.IterateThroughResultsForUser(self.result_map, True)
    expected_contents = validate_tag_consistency.TAG_HEADER + """\
[ win ] some_test [ Failure ]
[ mac ] some_test [ Failure ]
[ android ] some_test [ Failure ]
"""
    with open(self.expectation_file) as infile:
      self.assertEqual(infile.read(), expected_contents)

  def testIterateThroughResultsForUserRetryNoGroupByTags(self):
    """Tests that everything appears to function with retry and no group."""
    self._input_mock.return_value = ('RetryOnFailure', '')
    expectations.IterateThroughResultsForUser(self.result_map, False)
    expected_contents = validate_tag_consistency.TAG_HEADER + """\
[ win ] some_test [ Failure ]
[ mac ] some_test [ Failure ]
[ android ] some_test [ Failure ]
[ win ] foo_test [ RetryOnFailure ]
[ mac ] foo_test [ RetryOnFailure ]
[ win ] bar_test [ RetryOnFailure ]
"""
    with open(self.expectation_file) as infile:
      self.assertEqual(infile.read(), expected_contents)

  def testIterateThroughResultsForUserRetryGroupByTags(self):
    """Tests that everything appears to function with retry and grouping."""
    self._input_mock.return_value = ('RetryOnFailure', 'crbug.com/1')
    expectations.IterateThroughResultsForUser(self.result_map, True)
    expected_contents = validate_tag_consistency.TAG_HEADER + """\
[ win ] some_test [ Failure ]
crbug.com/1 [ win ] foo_test [ RetryOnFailure ]
crbug.com/1 [ win ] bar_test [ RetryOnFailure ]
[ mac ] some_test [ Failure ]
crbug.com/1 [ mac ] foo_test [ RetryOnFailure ]
[ android ] some_test [ Failure ]
"""
    with open(self.expectation_file) as infile:
      self.assertEqual(infile.read(), expected_contents)

  def testIterateThroughResultsForUserFailNoGroupByTags(self):
    """Tests that everything appears to function with failure and no group."""
    self._input_mock.return_value = ('Failure', 'crbug.com/1')
    expectations.IterateThroughResultsForUser(self.result_map, False)
    expected_contents = validate_tag_consistency.TAG_HEADER + """\
[ win ] some_test [ Failure ]
[ mac ] some_test [ Failure ]
[ android ] some_test [ Failure ]
crbug.com/1 [ win ] foo_test [ Failure ]
crbug.com/1 [ mac ] foo_test [ Failure ]
crbug.com/1 [ win ] bar_test [ Failure ]
"""
    with open(self.expectation_file) as infile:
      self.assertEqual(infile.read(), expected_contents)

  def testIterateThroughResultsForUserFailGroupByTags(self):
    """Tests that everything appears to function with failure and grouping."""
    self._input_mock.return_value = ('Failure', '')
    expectations.IterateThroughResultsForUser(self.result_map, True)
    expected_contents = validate_tag_consistency.TAG_HEADER + """\
[ win ] some_test [ Failure ]
[ win ] foo_test [ Failure ]
[ win ] bar_test [ Failure ]
[ mac ] some_test [ Failure ]
[ mac ] foo_test [ Failure ]
[ android ] some_test [ Failure ]
"""
    with open(self.expectation_file) as infile:
      self.assertEqual(infile.read(), expected_contents)


@unittest.skipIf(sys.version_info[0] != 3, 'Python 3-only')
class FindFailuresInSameConditionUnittest(unittest.TestCase):
  def setUp(self):
    self.result_map = {
        'pixel_integration_test': {
            'foo_test': {
                tuple(['win']): ['a'],
                tuple(['mac']): ['a', 'b'],
            },
            'bar_test': {
                tuple(['win']): ['a', 'b', 'c'],
                tuple(['mac']): ['a', 'b', 'c', 'd'],
            },
        },
        'webgl_conformance_integration_test': {
            'foo_test': {
                tuple(['win']): ['a', 'b', 'c', 'd', 'e'],
                tuple(['mac']): ['a', 'b', 'c', 'd', 'e', 'f'],
            },
            'bar_test': {
                tuple(['win']): ['a', 'b', 'c', 'd', 'e', 'f', 'g'],
                tuple(['mac']): ['a', 'b', 'c', 'd', 'e', 'f', 'g', 'h'],
            },
        },
    }

  def testFindFailuresInSameTest(self):
    other_failures = expectations.FindFailuresInSameTest(
        self.result_map, 'pixel_integration_test', 'foo_test', ['win'])
    self.assertEqual(other_failures, [(tuple(['mac']), 2)])

  def testFindFailuresInSameConfig(self):
    typ_tag_ordered_result_map = expectations._ReorderMapByTypTags(
        self.result_map)
    other_failures = expectations.FindFailuresInSameConfig(
        typ_tag_ordered_result_map, 'pixel_integration_test', 'foo_test',
        ['win'])
    expected_other_failures = [
        ('pixel_integration_test.bar_test', 3),
        ('webgl_conformance_integration_test.foo_test', 5),
        ('webgl_conformance_integration_test.bar_test', 7),
    ]
    self.assertEqual(len(other_failures), len(expected_other_failures))
    self.assertEqual(set(other_failures), set(expected_other_failures))


@unittest.skipIf(sys.version_info[0] != 3, 'Python 3-only')
class ModifyFileForResultUnittest(fake_filesystem_unittest.TestCase):
  def setUp(self):
    self.setUpPyfakefs()
    self.expectation_file = os.path.join(
        expectations.ABSOLUTE_EXPECTATION_FILE_DIRECTORY, 'expectation.txt')
    uu.CreateFile(self, self.expectation_file)
    self._expectation_file_patcher = mock.patch.object(
        expectations, 'GetExpectationFileForSuite')
    self._expectation_file_mock = self._expectation_file_patcher.start()
    self.addCleanup(self._expectation_file_patcher.stop)
    self._expectation_file_mock.return_value = self.expectation_file

  def testNoGroupByTags(self):
    """Tests that not grouping by tags appends to the end."""
    expectation_file_contents = validate_tag_consistency.TAG_HEADER + """\
[ win ] some_test [ Failure ]

[ mac ] some_test [ Failure ]
"""
    with open(self.expectation_file, 'w') as outfile:
      outfile.write(expectation_file_contents)
    expectations.ModifyFileForResult(None, 'some_test', ['win', 'win10'], '',
                                     'Failure', False)
    expected_contents = validate_tag_consistency.TAG_HEADER + """\
[ win ] some_test [ Failure ]

[ mac ] some_test [ Failure ]
[ win win10 ] some_test [ Failure ]
"""
    with open(self.expectation_file) as infile:
      self.assertEqual(infile.read(), expected_contents)

  def testGroupByTagsNoMatch(self):
    """Tests that grouping by tags but finding no match appends to the end."""
    expectation_file_contents = validate_tag_consistency.TAG_HEADER + """\
[ mac ] some_test [ Failure ]
"""
    with open(self.expectation_file, 'w') as outfile:
      outfile.write(expectation_file_contents)
    expectations.ModifyFileForResult(None, 'some_test', ['win', 'win10'], '',
                                     'Failure', True)
    expected_contents = validate_tag_consistency.TAG_HEADER + """\
[ mac ] some_test [ Failure ]
[ win win10 ] some_test [ Failure ]
"""
    with open(self.expectation_file) as infile:
      self.assertEqual(infile.read(), expected_contents)

  def testGroupByTagsMatch(self):
    """Tests that grouping by tags and finding a match adds mid-file."""
    expectation_file_contents = validate_tag_consistency.TAG_HEADER + """\
[ win ] some_test [ Failure ]

[ mac ] some_test [ Failure ]
"""
    with open(self.expectation_file, 'w') as outfile:
      outfile.write(expectation_file_contents)
    expectations.ModifyFileForResult(None, 'foo_test', ['win', 'win10'], '',
                                     'Failure', True)
    expected_contents = validate_tag_consistency.TAG_HEADER + """\
[ win ] some_test [ Failure ]
[ win ] foo_test [ Failure ]

[ mac ] some_test [ Failure ]
"""
    with open(self.expectation_file) as infile:
      self.assertEqual(infile.read(), expected_contents)


@unittest.skipIf(sys.version_info[0] != 3, 'Python 3-only')
class GetExpectationFileForSuiteUnittest(unittest.TestCase):
  def testRegularExpectationFile(self):
    """Tests that a regular expectation file is found properly."""
    expected_filepath = os.path.join(
        expectations.ABSOLUTE_EXPECTATION_FILE_DIRECTORY,
        'pixel_expectations.txt')
    actual_filepath = expectations.GetExpectationFileForSuite(
        'pixel_integration_test', ['webgl-version-2'])
    self.assertEqual(actual_filepath, expected_filepath)

  def testOverrideExpectationFile(self):
    """Tests that an overridden expectation file is found properly."""
    expected_filepath = os.path.join(
        expectations.ABSOLUTE_EXPECTATION_FILE_DIRECTORY,
        'info_collection_expectations.txt')
    actual_filepath = expectations.GetExpectationFileForSuite(
        'info_collection_test', ['webgl-version-2'])
    self.assertEqual(actual_filepath, expected_filepath)

  def testWebGl1Conformance(self):
    """Tests that a WebGL 1 expectation file is found properly."""
    expected_filepath = os.path.join(
        expectations.ABSOLUTE_EXPECTATION_FILE_DIRECTORY,
        'webgl_conformance_expectations.txt')
    actual_filepath = expectations.GetExpectationFileForSuite(
        'webgl_conformance_integration_test', [])
    self.assertEqual(actual_filepath, expected_filepath)

  def testWebGl2Conformance(self):
    """Tests that a WebGL 2 expectation file is found properly."""
    expected_filepath = os.path.join(
        expectations.ABSOLUTE_EXPECTATION_FILE_DIRECTORY,
        'webgl2_conformance_expectations.txt')
    actual_filepath = expectations.GetExpectationFileForSuite(
        'webgl_conformance_integration_test', ['webgl-version-2'])
    self.assertEqual(actual_filepath, expected_filepath)


@unittest.skipIf(sys.version_info[0] != 3, 'Python 3-only')
class FindBestInsertionLineForExpectationUnittest(
    fake_filesystem_unittest.TestCase):
  def setUp(self):
    self.setUpPyfakefs()
    self.expectation_file = os.path.join(
        expectations.ABSOLUTE_EXPECTATION_FILE_DIRECTORY, 'expectation.txt')
    uu.CreateFile(self, self.expectation_file)
    expectation_file_contents = validate_tag_consistency.TAG_HEADER + """\
[ win ] some_test [ Failure ]

[ mac ] some_test [ Failure ]

[ win release ] bar_test [ Failure ]
[ win ] foo_test [ Failure ]

[ chromeos ] some_test [ Failure ]
"""
    with open(self.expectation_file, 'w') as outfile:
      outfile.write(expectation_file_contents)

  def testNoMatchingTags(self):
    """Tests behavior when there are no expectations with matching tags."""
    insertion_line, tags = expectations.FindBestInsertionLineForExpectation(
        ['android'], self.expectation_file)
    self.assertEqual(insertion_line, -1)
    self.assertEqual(tags, set())

  def testMatchingTagsLastEntryChosen(self):
    """Tests that the last matching line is chosen."""
    insertion_line, tags = expectations.FindBestInsertionLineForExpectation(
        ['win'], self.expectation_file)
    # We expect "[ win ] foo_test [ Failure ]" to be chosen
    expected_line = len(validate_tag_consistency.TAG_HEADER.splitlines()) + 6
    self.assertEqual(insertion_line, expected_line)
    self.assertEqual(tags, set(['win']))

  def testMatchingTagsClosestMatchChosen(self):
    """Tests that the closest tag match is chosen."""
    insertion_line, tags = expectations.FindBestInsertionLineForExpectation(
        ['win', 'release'], self.expectation_file)
    # We expect "[ win release ] bar_test [ Failure ]" to be chosen
    expected_line = len(validate_tag_consistency.TAG_HEADER.splitlines()) + 5
    self.assertEqual(insertion_line, expected_line)
    self.assertEqual(tags, set(['win', 'release']))


class GetExpectationFilesFromOriginUnittest(unittest.TestCase):
  class FakeRequestResult(object):
    def __init__(self):
      self.status_code = 200
      self.text = ''

  def setUp(self):
    self._get_patcher = mock.patch('flake_suppressor.expectations.requests.get')
    self._get_mock = self._get_patcher.start()
    self.addCleanup(self._get_patcher.stop)

  def testBasic(self):
    """Tests basic functionality along the happy path."""

    def SideEffect(url):
      request_result = GetExpectationFilesFromOriginUnittest.FakeRequestResult()
      text = ''
      if url.endswith('test_expectations?format=TEXT'):
        text = """\
mode type hash foo_tests.txt
mode type hash bar_tests.txt"""
      elif url.endswith('foo_tests.txt?format=TEXT'):
        text = 'foo_tests.txt content'
      elif url.endswith('bar_tests.txt?format=TEXT'):
        text = 'bar_tests.txt content'
      else:
        self.fail('Given unhandled URL %s' % url)
      request_result.text = base64.b64encode(text.encode('utf-8'))
      return request_result

    self._get_mock.side_effect = SideEffect
    expected_contents = {
        'foo_tests.txt': 'foo_tests.txt content',
        'bar_tests.txt': 'bar_tests.txt content',
    }
    self.assertEqual(expectations.GetExpectationFilesFromOrigin(),
                     expected_contents)
    self.assertEqual(self._get_mock.call_count, 3)

  def testNonOkStatusCodesSurfaced(self):
    """Tests that getting a non-200 status code back results in a failure."""

    def SideEffect(_):
      request_result = GetExpectationFilesFromOriginUnittest.FakeRequestResult()
      request_result.status_code = 404
      return request_result

    self._get_mock.side_effect = SideEffect
    with self.assertRaises(AssertionError):
      expectations.GetExpectationFilesFromOrigin()


class GetExpectationFilesFromLocalCheckoutUnittest(
    fake_filesystem_unittest.TestCase):
  def setUp(self):
    self.setUpPyfakefs()

  def testBasic(self):
    """Tests basic functionality."""
    os.makedirs(expectations.ABSOLUTE_EXPECTATION_FILE_DIRECTORY)
    with open(
        os.path.join(expectations.ABSOLUTE_EXPECTATION_FILE_DIRECTORY,
                     'foo.txt'), 'w') as outfile:
      outfile.write('foo.txt contents')
    with open(
        os.path.join(expectations.ABSOLUTE_EXPECTATION_FILE_DIRECTORY,
                     'bar.txt'), 'w') as outfile:
      outfile.write('bar.txt contents')
    expected_contents = {
        'foo.txt': 'foo.txt contents',
        'bar.txt': 'bar.txt contents',
    }
    self.assertEqual(expectations.GetExpectationFilesFromLocalCheckout(),
                     expected_contents)


class AssertCheckoutIsUpToDateUnittest(unittest.TestCase):
  def setUp(self):
    self._origin_patcher = mock.patch(
        'flake_suppressor.expectations.GetExpectationFilesFromOrigin')
    self._origin_mock = self._origin_patcher.start()
    self.addCleanup(self._origin_patcher.stop)
    self._local_patcher = mock.patch(
        'flake_suppressor.expectations.GetExpectationFilesFromLocalCheckout')
    self._local_mock = self._local_patcher.start()
    self.addCleanup(self._local_patcher.stop)

  def testContentsMatch(self):
    """Tests the happy path where the contents match."""
    self._origin_mock.return_value = {
        'foo.txt': 'foo_content',
        'bar.txt': 'bar_content',
    }
    self._local_mock.return_value = {
        'bar.txt': 'bar_content',
        'foo.txt': 'foo_content',
    }
    expectations.AssertCheckoutIsUpToDate()

  def testContentsDoNotMatch(self):
    """Tests that mismatched contents results in a failure."""
    self._origin_mock.return_value = {
        'foo.txt': 'foo_content',
        'bar.txt': 'bar_content',
    }
    # Differing keys.
    self._local_mock.return_value = {
        'bar.txt': 'bar_content',
        'foo2.txt': 'foo_content',
    }
    with self.assertRaises(RuntimeError):
      expectations.AssertCheckoutIsUpToDate()

    # Differing values.
    self._local_mock.return_value = {
        'bar.txt': 'bar_content',
        'foo.txt': 'foo_content2',
    }
    with self.assertRaises(RuntimeError):
      expectations.AssertCheckoutIsUpToDate()


if __name__ == '__main__':
  unittest.main(verbosity=2)
