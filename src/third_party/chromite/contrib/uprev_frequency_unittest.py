# -*- coding: utf-8 -*-
# Copyright 2019 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Unit tests for the uprev_frequency script."""

from __future__ import print_function

import datetime
import mock

from chromite.contrib import uprev_frequency
from chromite.lib import cros_test_lib
from chromite.lib import git
from chromite.scripts import cros_mark_as_stable


class GetDirectoryCommitsTest(cros_test_lib.MockTestCase):
  """Unit tests for get_directory_commits."""

  def testNoOutput(self):
    """Test get_directory_commits doesn't explode when log has no output."""
    self.PatchObject(git, 'Log', return_value='')
    self.assertFalse(uprev_frequency.get_directory_commits('foo/bar'))

  def testGitLogCalledCorrectly(self):
    """Test get_directory_commits calls git.Log with correct arguments."""
    start_date = datetime.datetime.strptime('1996-01-01',
                                            uprev_frequency.DATE_FORMAT)
    end_date = datetime.datetime.strptime('1997-01-01',
                                          uprev_frequency.DATE_FORMAT)

    log = self.PatchObject(git, 'Log', return_value='')

    uprev_frequency.get_directory_commits('foo/bar', start_date=start_date,
                                          end_date=end_date)

    self.assertEqual(log.call_args_list, [
        mock.call('foo/bar', format='format:"%h|%cd|%s"',
                  after='1996-01-01', until='1997-01-01', reverse=True,
                  date='unix', paths=['foo/bar'])
    ])

  def testCommitParsing(self):
    """Test get_directory_commits when log outputs commits."""
    log_lines = [
        'abc|123|foo',
        'def|456|bar',
    ]
    log_output = '\n'.join(log_lines)
    self.PatchObject(git, 'Log', return_value=log_output)

    commits = uprev_frequency.get_directory_commits('foo/bar')
    self.assertEqual(commits, [
        uprev_frequency.Commit('abc', '123', 'foo'),
        uprev_frequency.Commit('def', '456', 'bar'),
    ])


class GetUprevCommitsTest(cros_test_lib.TestCase):
  """Unit tests for get_uprev_commits."""

  def testEmptyInput(self):
    """Test get_uprev_commits does not explode on empty list."""
    self.assertFalse(uprev_frequency.get_uprev_commits([]))

  def testMixed(self):
    """Test get_uprev_commits with mixed input."""
    uprev_commit = uprev_frequency.Commit(
        'abc', '123', cros_mark_as_stable.GIT_COMMIT_SUBJECT)
    commits = [
        uprev_commit,
        uprev_frequency.Commit('def', '456', 'not an uprev commit'),
    ]
    self.assertEqual(uprev_frequency.get_uprev_commits(commits), [uprev_commit])


class GetCommitTimestampsTest(cros_test_lib.TestCase):
  """Unit tests for get_commit_timestamps."""

  def testMalformed(self):
    """Test get_commit_timestamps explodes on malformed timestamp."""
    self.assertRaises(ValueError, uprev_frequency.get_commit_timestamps,
                      [uprev_frequency.Commit('abc', 'bad-timestamp', 'foo')])


  def testBasic(self):
    """Test get_commit_timestamps works with good timestamps."""
    commits = [
        uprev_frequency.Commit('abc', '123', 'foo'),
        uprev_frequency.Commit('def', '456', 'bar'),
    ]
    timestamps = uprev_frequency.get_commit_timestamps(commits)
    self.assertEqual(timestamps, [123, 456])


class GetAverageTimestampDeltaDaysTest(cros_test_lib.TestCase):
  """Unit tests for get_average_timestamp_delta_days."""

  def testEmptyInput(self):
    """Test get_average_timestamp_delta_days dies on empty input."""
    self.assertRaises(
        ValueError, uprev_frequency.get_average_timestamp_delta_days, [])

  def testOneTimestampInput(self):
    """Test get_average_timestamp_delta_days dies on single timestamp."""
    self.assertRaises(
        ValueError, uprev_frequency.get_average_timestamp_delta_days, [1])

  def testMultipleTimestampsInput(self):
    """Test get_average_timestamp_delta_days computes correct average delta."""
    timestamps = [
        1 * uprev_frequency.SECONDS_PER_DAY,
        3 * uprev_frequency.SECONDS_PER_DAY,
        5 * uprev_frequency.SECONDS_PER_DAY,
    ]
    average_delta = uprev_frequency.get_average_timestamp_delta_days(timestamps)
    self.assertEqual(average_delta, 2)
