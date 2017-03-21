# Copyright 2017 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Git repo metrics."""

from __future__ import absolute_import
from __future__ import print_function
from __future__ import unicode_literals

import psutil

import os
import subprocess

from chromite.lib import cros_logging as logging
from infra_libs import ts_mon

logger = logging.getLogger(__name__)


class _GitRepo(object):
  """Helper class for running git commands."""

  def __init__(self, gitdir):
    self._gitdir = gitdir

  def _get_git_command(self):
    return ['git', '--git-dir', self._gitdir]

  def _check_output(self, args, **kwargs):
    return subprocess.check_output(
        self._get_git_command() + list(args), **kwargs)

  def get_commit_hash(self):
    return self._check_output(['rev-parse', 'HEAD']).strip()

  def get_commit_time(self):
    return int(self._check_output(['show', '-s', '--format=%ct', 'HEAD'])
               .strip())


class _GitMetricCollector(object):
  """Class for collecting metrics about a git repository."""

  _commit_hash_metric = ts_mon.StringMetric(
      'git/hash',
      description='Current Git commit hash.')

  _commit_time_metric = ts_mon.GaugeMetric(
      'git/commit_time',
      description='Current Git commit time as seconds since Unix Epoch.')

  def __init__(self, gitdir, metric_path):
    self._gitdir = gitdir
    self._gitrepo = _GitRepo(gitdir)
    self._fields = {'repo': gitdir}
    self._metric_path = metric_path

  def collect(self):
    """Collect metrics."""
    try:
      self._collect_commit_hash_metric()
      self._collect_commit_time_metric()
    except subprocess.CalledProcessError as e:
      logger.warning('Error collecting git metrics for %s: %s',
                     self._gitdir, e)

  def _collect_commit_hash_metric(self):
    commit_hash = self._gitrepo.get_commit_hash()
    logger.debug('Collecting Git hash %r for %r', commit_hash, self._gitdir)
    self._commit_hash_metric.set(commit_hash, self._fields)

  def _collect_commit_time_metric(self):
    commit_time = self._gitrepo.get_commit_time()
    logger.debug('Collecting Git commit time %r for %r',
                 commit_time, self._gitdir)
    self._commit_time_metric.set(commit_time, self._fields)


_CHROMIUMOS_DIR = os.path.expanduser('~chromeos-test/chromiumos/')

_repo_collectors = (
  # TODO(ayatane): We cannot access chromeos-admin because we are
  # running as non-root.
  _GitMetricCollector(gitdir='/root/chromeos-admin/.git',
                      metric_path='chromeos-admin'),
  _GitMetricCollector(gitdir=_CHROMIUMOS_DIR + 'chromite/.git',
                      metric_path='chromite'),
  _GitMetricCollector(gitdir='/usr/local/autotest/.git',
                      metric_path='installed_autotest'),
)


def collect_git_metrics():
  """Collect metrics for Git repository state."""
  for collector in _repo_collectors:
    collector.collect()
