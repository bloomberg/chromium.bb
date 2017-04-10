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

class TestGitMetrics(cros_test_lib.TempDirTestCase):
  """Tests for git metrics."""

  def setUp(self):
    self._InitRepo()

  def _InitRepo(self):
    """Initializes a repo in the temp directory."""
    self.git_dir = os.path.join(self.tempdir, '.git')
    # with osutils.ChdirContext(self.tempdir):
    self.git_repo = git_metrics._GitRepo(self.git_dir)
    self.git_repo._check_output(['init'])
    self.git_repo._check_output(['commit', '--allow-empty', '-m', 'hi'])

  def testCollectGitHashTypesAreCorrect(self):
    """Tests that collecting the git hash doesn't have type conflicts."""
    collector = git_metrics._GitMetricCollector(self.git_dir, '/foo/bar')

    # This has the side-effect of checking the types are correct.
    collector._collect_commit_hash_metric()

  def testCollectGitTimeTypesAreCorrect(self):
    """Tests that collecting the git commit time works."""
    collector = git_metrics._GitMetricCollector(self.git_dir, '/foo/bar')

    # This has the side-effect of checking the types are correct.
    collector._collect_timestamp_metric()

  def testCollectGitHashCallsSet(self):
    collector = git_metrics._GitMetricCollector(self.git_dir, '/foo/bar')

    with mock.patch.object(git_metrics._GitMetricCollector,
                           '_commit_hash_metric',
                           autospec=True) as hash_metric:
      collector._collect_commit_hash_metric()

    commit_hash = self.git_repo.get_commit_hash()
    self.assertEqual(hash_metric.set.call_args_list, [
        mock.call(commit_hash, {'repo': self.git_dir})
    ])

  def testCollectGitTimeCallsSet(self):
    collector = git_metrics._GitMetricCollector(self.git_dir, '/foo/bar')
    with mock.patch.object(git_metrics._GitMetricCollector,
                           '_timestamp_metric',
                           autospec=True) as time_metric:
      collector._collect_timestamp_metric()

    timestamp = self.git_repo.get_commit_time()
    self.assertEqual(time_metric.set.call_args_list, [
        mock.call(timestamp, {'repo': self.git_dir})
    ])
