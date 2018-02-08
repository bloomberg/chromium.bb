#!/usr/bin/env python
# Copyright 2016 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# This file was copy-pasted over from:
# //build/scripts/slave/upload_perf_dashboard_results.py
# with sections copied from:
# //build/scripts/slave/slave_utils.py

import json
import optparse
import os
import re
import subprocess
import sys

from core import results_dashboard


def _GetMainRevision(commit_pos, build_dir):
  """Return revision to use as the numerical x-value in the perf dashboard.
  This will be used as the value of "rev" in the data passed to
  results_dashboard.SendResults.
  In order or priority, this function could return:
    1. The value of "got_revision_cp" in build properties.
    3. An SVN number, git commit position, or git commit hash.
  """
  if commit_pos is not None:
    return int(re.search(r'{#(\d+)}', commit_pos).group(1))
  # TODO(sullivan,qyearsley): Don't fall back to _GetRevision if it returns
  # a git commit, since this should be a numerical revision. Instead, abort
  # and fail.
  return _GetRevision(os.path.dirname(os.path.abspath(build_dir)))


def _GetDashboardJson(options):
  main_revision = _GetMainRevision(options.got_revision_cp, options.build_dir)
  revisions = _GetPerfDashboardRevisionsWithProperties(
    options.got_webrtc_revision, options.got_v8_revision, options.version,
    options.git_revision, main_revision)
  reference_build = 'reference' in options.name
  stripped_test_name = options.name.replace('.reference', '')
  results = {}
  with open(options.results_file) as f:
    results = json.load(f)
  dashboard_json = {}
  if not 'charts' in results:
    # These are legacy results.
    dashboard_json = results_dashboard.MakeListOfPoints(
      results, options.configuration.name, stripped_test_name,
      options.buildername, options.buildnumber, {},
      _GetMachineGroup(options), revisions_dict=revisions)
  else:
    dashboard_json = results_dashboard.MakeDashboardJsonV1(
      results,
      revisions, stripped_test_name, options.configuration_name,
      options.buildername, options.buildnumber,
      {}, reference_build,
      perf_dashboard_machine_group=_GetMachineGroup(options))
  return dashboard_json

def _GetMachineGroup(options):
  perf_dashboard_machine_group = options.perf_dashboard_machine_group
  if options.is_luci_builder and not perf_dashboard_machine_group:
    raise ValueError(
        "Luci builder must set 'perf_dashboard_machine_group'. See "
        'bit.ly/perf-dashboard-machine-group for more details')
  elif not options.is_luci_builder:
    # TODO(crbug.com/801289):
    # Remove this code path once all builders are converted to LUCI.
    # perf_dashboard_machine_group = chromium_utils.GetActiveMaster()
    # hardcoding the result of this line for now
    perf_dashboard_machine_group = 'ChromiumPerfFyi'
  return perf_dashboard_machine_group


def _GetDashboardHistogramData(options):
  revisions = {
      '--chromium_commit_positions': _GetMainRevision(
          options.got_revision_cp, options.build_dir),
      '--chromium_revisions': options.git_revision
  }

  if options.got_webrtc_revision:
    revisions['--webrtc_revisions'] = options.got_webrtc_revision
  if options.got_v8_revision:
    revisions['--v8_revisions'] = options.got_v8_revision

  is_reference_build = 'reference' in options.name
  stripped_test_name = options.name.replace('.reference', '')

  return results_dashboard.MakeHistogramSetWithDiagnostics(
      options.results_file, options.chromium_checkout_dir, stripped_test_name,
      options.configuration_name, options.buildername, options.buildnumber,
      revisions, is_reference_build,
      perf_dashboard_machine_group=_GetMachineGroup(options))


def _CreateParser():
  # Parse options
  parser = optparse.OptionParser()
  parser.add_option('--name')
  parser.add_option('--results-file')
  parser.add_option('--output-json-file')
  parser.add_option('--got-revision-cp')
  parser.add_option('--build-dir')
  parser.add_option('--configuration-name')
  parser.add_option('--results-url')
  parser.add_option('--is-luci-builder', action='store_true', default=False)
  parser.add_option('--perf-dashboard-machine-group')
  parser.add_option('--buildername')
  parser.add_option('--buildnumber')
  parser.add_option('--got-webrtc-revision')
  parser.add_option('--got-v8-revision')
  parser.add_option('--version')
  parser.add_option('--git-revision')
  parser.add_option('--output-json-dashboard-url')
  parser.add_option('--send-as-histograms', action='store_true')
  parser.add_option('--oauth-token-file')
  parser.add_option('--chromium-checkout-dir')
  return parser


def main(args):
  parser = _CreateParser()
  options, extra_args = parser.parse_args(args)

  # Validate options.
  if extra_args:
    parser.error('Unexpected command line arguments')
  if not options.configuration_name or not options.results_url:
    parser.error('configuration_name and results_url are required.')

  if options.oauth_token_file:
    with open(options.oauth_token_file) as f:
      oauth_token = f.readline()
  else:
    oauth_token = None

  if not options.send_as_histograms:
    dashboard_json = _GetDashboardJson(options)
  else:
    dashboard_json = _GetDashboardHistogramData(options)

  if dashboard_json:
    if options.output_json_file:
      with open(options.output_json_file, 'w') as output_file:
        json.dump(dashboard_json, output_file)
    if not results_dashboard.SendResults(
        dashboard_json,
        options.results_url,
        options.build_dir,
        options.output_json_dashboard_url,
        send_as_histograms=options.send_as_histograms,
        oauth_token=oauth_token):
      return 1
  else:
    print 'Error: No perf dashboard JSON was produced.'
    print '@@@STEP_FAILURE@@@'
    return 1
  return 0


if __name__ == '__main__':
  sys.exit(main((sys.argv[1:])))


def _GetRevision(in_directory):
  """Returns the SVN revision, git commit position, or git hash.

  Args:
    in_directory: A directory in the repository to be checked.

  Returns:
    An SVN revision as a string if the given directory is in a SVN repository,
    or a git commit position number, or if that's not available, a git hash.
    If all of that fails, an empty string is returned.
  """
  if not os.path.exists(os.path.join(in_directory, '.svn')):
    if _IsGitDirectory(in_directory):
      svn_rev = _GetGitCommitPosition(in_directory)
      if svn_rev:
        return svn_rev
      return _GetGitRevision(in_directory)
    else:
      return ''


def _IsGitDirectory(dir_path):
  """Checks whether the given directory is in a git repository.

  Args:
    dir_path: The directory path to be tested.

  Returns:
    True if given directory is in a git repository, False otherwise.
  """
  git_exe = 'git.bat' if sys.platform.startswith('win') else 'git'
  with open(os.devnull, 'w') as devnull:
    p = subprocess.Popen([git_exe, 'rev-parse', '--git-dir'],
                         cwd=dir_path, stdout=devnull, stderr=devnull)
    return p.wait() == 0


# Regex matching git comment lines containing svn revision info.
GIT_SVN_ID_RE = re.compile(r'^git-svn-id: .*@([0-9]+) .*$')
# Regex for the master branch commit position.
GIT_CR_POS_RE = re.compile(r'^Cr-Commit-Position: refs/heads/master@{#(\d+)}$')


def _GetGitCommitPositionFromLog(log):
  """Returns either the commit position or svn rev from a git log."""
  # Parse from the bottom up, in case the commit message embeds the message
  # from a different commit (e.g., for a revert).
  for r in [GIT_CR_POS_RE, GIT_SVN_ID_RE]:
    for line in reversed(log.splitlines()):
      m = r.match(line.strip())
      if m:
        return m.group(1)
  return None


def _GetGitCommitPosition(dir_path):
  """Extracts the commit position or svn revision number of the HEAD commit."""
  git_exe = 'git.bat' if sys.platform.startswith('win') else 'git'
  p = subprocess.Popen(
      [git_exe, 'log', '-n', '1', '--pretty=format:%B', 'HEAD'],
      cwd=dir_path, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
  (log, _) = p.communicate()
  if p.returncode != 0:
    return None
  return _GetGitCommitPositionFromLog(log)


def _GetPerfDashboardRevisionsWithProperties(
    got_webrtc_revision, got_v8_revision, version, git_revision, main_revision,
    point_id=None):
  """Fills in the same revisions fields that process_log_utils does."""

  versions = {}
  versions['rev'] = main_revision
  versions['webrtc_rev'] = got_webrtc_revision
  versions['v8_rev'] = got_v8_revision
  versions['ver'] = version
  versions['git_revision'] = git_revision
  versions['point_id'] = point_id
  # There are a lot of "bad" revisions to check for, so clean them all up here.
  for key in versions.keys():
    if not versions[key] or versions[key] == 'undefined':
      del versions[key]
  return versions


def _GetGitRevision(in_directory):
  """Returns the git hash tag for the given directory.

  Args:
    in_directory: The directory where git is to be run.

  Returns:
    The git SHA1 hash string.
  """
  git_exe = 'git.bat' if sys.platform.startswith('win') else 'git'
  p = subprocess.Popen(
      [git_exe, 'rev-parse', 'HEAD'],
      cwd=in_directory, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
  (stdout, _) = p.communicate()
  return stdout.strip()
