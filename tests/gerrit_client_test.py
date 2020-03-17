#!/usr/bin/env vpython3
# coding=utf-8
# Copyright 2020 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Unit tests for gerrit_client.py."""

import logging
import os
import sys
import unittest

if sys.version_info.major == 2:
  from StringIO import StringIO
  import mock
else:
  from io import StringIO
  from unittest import mock

sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))

import gerrit_client
import gerrit_util


class TestGerritClient(unittest.TestCase):
  @mock.patch('gerrit_util.GetGerritBranch', return_value='')
  def test_branch_info(self, util_mock):
    gerrit_client.main([
        'branchinfo', '--host', 'https://example.org/foo', '--project',
        'projectname', '--branch', 'branchname'
    ])
    util_mock.assert_called_once_with('example.org', 'projectname',
                                      'branchname')

  @mock.patch('gerrit_util.CreateGerritBranch', return_value='')
  def test_branch(self, util_mock):
    gerrit_client.main([
        'branch', '--host', 'https://example.org/foo', '--project',
        'projectname', '--branch', 'branchname', '--commit', 'commitname'
    ])
    util_mock.assert_called_once_with('example.org', 'projectname',
                                      'branchname', 'commitname')

  @mock.patch('gerrit_util.QueryChanges', return_value='')
  def test_changes(self, util_mock):
    gerrit_client.main([
        'changes', '--host', 'https://example.org/foo', '-p', 'foo=bar', '-p',
        'baz=qux', '--limit', '10', '--start', '20', '-o', 'op1', '-o', 'op2'
    ])
    util_mock.assert_called_once_with(
        'example.org', [('foo', 'bar'), ('baz', 'qux')],
        limit=10,
        start=20,
        o_params=['op1', 'op2'])

  @mock.patch('gerrit_util.AbandonChange', return_value='')
  def test_abandon(self, util_mock):
    gerrit_client.main([
        'abandon', '--host', 'https://example.org/foo', '-c', '1', '-m', 'bar'
    ])
    util_mock.assert_called_once_with('example.org', 1, 'bar')


if __name__ == '__main__':
  logging.basicConfig(
      level=logging.DEBUG if '-v' in sys.argv else logging.ERROR)
  unittest.main()
