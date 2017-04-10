# Copyright 2016 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Unit tests for git_metrics."""

from __future__ import absolute_import
from __future__ import print_function
from __future__ import unicode_literals

import mock
import os

from chromite.lib import cros_test_lib, osutils
from chromite.scripts.sysmon import git_metrics

# pylint: disable=protected-access
# pylint: disable=attribute-defined-outside-init

class TestGitMetricsWithTempdir(cros_test_lib.TempDirTestCase):
  """Tests for git metrics using a Git fixture."""

  def setUp(self):
    self.git_dir = os.path.join(self.tempdir, '.git')
    with osutils.ChdirContext(self.tempdir):
      self.git_repo = git_metrics._GitRepo(self.git_dir)
      self.git_repo._check_output(['init'])
      self.git_repo._check_output(['commit', '--allow-empty', '-m', 'hi'])

  def test_collect_git_hash_types_are_correct(self):
    """Tests that collecting the git hash doesn't have type conflicts."""
    collector = git_metrics._GitMetricCollector(self.git_dir, '/foo/bar')

    # This has the side-effect of checking the types are correct.
    collector._collect_commit_hash_metric()

  def test_collect_git_time_types_are_correct(self):
    """Tests that collecting the git commit time works."""
    collector = git_metrics._GitMetricCollector(self.git_dir, '/foo/bar')

    # This has the side-effect of checking the types are correct.
    collector._collect_timestamp_metric()

  def test_collect_git_hash_calls_set(self):
    collector = git_metrics._GitMetricCollector(self.git_dir, '/foo/bar')

    with mock.patch.object(git_metrics._GitMetricCollector,
                           '_commit_hash_metric',
                           autospec=True) as hash_metric:
      collector._collect_commit_hash_metric()

    commit_hash = self.git_repo.get_commit_hash()
    self.assertEqual(hash_metric.set.call_args_list, [
        mock.call(commit_hash, {'repo': self.git_dir})
    ])

  def test_collect_git_time_calls_set(self):
    collector = git_metrics._GitMetricCollector(self.git_dir, '/foo/bar')
    with mock.patch.object(git_metrics._GitMetricCollector,
                           '_timestamp_metric',
                           autospec=True) as time_metric:
      collector._collect_timestamp_metric()

    timestamp = self.git_repo.get_commit_time()
    self.assertEqual(time_metric.set.call_args_list, [
        mock.call(timestamp, {'repo': self.git_dir})
    ])


class TestMetricCollector(cros_test_lib.TestCase):
  """Tests for _GitMetricCollector."""

  def test__gitdir_expand_user(self):
    """Test that _gitdir is expanded for user."""
    with mock.patch.dict(os.environ, HOME='/home/arciel'):
      collector = git_metrics._GitMetricCollector('~/solciel', 'dummy')
    self.assertEqual(collector._gitdir, '~/solciel')
    self.assertEqual(collector._gitrepo._gitdir, '/home/arciel/solciel')
