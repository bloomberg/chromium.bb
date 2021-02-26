# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import contextlib
import hashlib

from recipe_engine import recipe_api


class TryserverApi(recipe_api.RecipeApi):
  def __init__(self, *args, **kwargs):
    super(TryserverApi, self).__init__(*args, **kwargs)
    self._gerrit_change = None  # self.m.buildbucket.common_pb2.GerritChange
    self._gerrit_change_repo_url = None

    self._gerrit_info_initialized = False
    self._gerrit_change_target_ref = None
    self._gerrit_change_fetch_ref = None
    self._gerrit_change_owner = None

  def initialize(self):
    changes = self.m.buildbucket.build.input.gerrit_changes
    if len(changes) == 1:
      cl = changes[0]
      self._gerrit_change = cl
      git_host = cl.host
      gs_suffix = '-review.googlesource.com'
      if git_host.endswith(gs_suffix):
        git_host = '%s.googlesource.com' % git_host[:-len(gs_suffix)]
      self._gerrit_change_repo_url = 'https://%s/%s' % (git_host, cl.project)

  @property
  def gerrit_change(self):
    """Returns current gerrit change, if there is exactly one.

    Returns a self.m.buildbucket.common_pb2.GerritChange or None.
    """
    return self._gerrit_change

  @property
  def gerrit_change_repo_url(self):
    """Returns canonical URL of the gitiles repo of the current Gerrit CL.

    Populated iff gerrit_change is populated.
    """
    return self._gerrit_change_repo_url

  @property
  def gerrit_change_owner(self):
    """Returns owner of the current Gerrit CL.

    Populated iff gerrit_change is populated.
    Is a dictionary with keys like "name".
    """
    return self._gerrit_change_owner

  def _ensure_gerrit_change_info(self):
    """Initializes extra info about gerrit_change, fetched from Gerrit server.

    Initializes _gerrit_change_target_ref and _gerrit_change_fetch_ref.

    May emit a step when called for the first time.
    """
    cl = self.gerrit_change
    if not cl:  # pragma: no cover
      return

    if self._gerrit_info_initialized:
      return

    td = self._test_data if self._test_data.enabled else {}
    mock_res = [{
      'branch': td.get('gerrit_change_target_ref', 'master'),
      'revisions': {
        '184ebe53805e102605d11f6b143486d15c23a09c': {
          '_number': str(cl.patchset),
          'ref': 'refs/changes/%02d/%d/%d' % (
              cl.change % 100, cl.change, cl.patchset),
        },
      },
      'owner': {
          'name': 'John Doe',
      },
    }]
    res = self.m.gerrit.get_changes(
        host='https://' + cl.host,
        query_params=[('change', cl.change)],
        # This list must remain static/hardcoded.
        # If you need extra info, either change it here (hardcoded) or
        # fetch separately.
        o_params=['ALL_REVISIONS', 'DOWNLOAD_COMMANDS'],
        limit=1,
        name='fetch current CL info',
        timeout=600,
        step_test_data=lambda: self.m.json.test_api.output(mock_res))[0]

    self._gerrit_change_target_ref = res['branch']
    if not self._gerrit_change_target_ref.startswith('refs/'):
      self._gerrit_change_target_ref = (
          'refs/heads/' + self._gerrit_change_target_ref)

    for rev in res['revisions'].values():
      if int(rev['_number']) == self.gerrit_change.patchset:
        self._gerrit_change_fetch_ref = rev['ref']
        break
    self._gerrit_change_owner = res['owner']
    self._gerrit_info_initialized = True

  @property
  def gerrit_change_fetch_ref(self):
    """Returns gerrit patch ref, e.g. "refs/heads/45/12345/6, or None.

    Populated iff gerrit_change is populated.
    """
    self._ensure_gerrit_change_info()
    return self._gerrit_change_fetch_ref

  @property
  def gerrit_change_target_ref(self):
    """Returns gerrit change destination ref, e.g. "refs/heads/master".

    Populated iff gerrit_change is populated.
    """
    self._ensure_gerrit_change_info()
    return self._gerrit_change_target_ref

  @property
  def is_tryserver(self):
    """Returns true iff we have a change to check out."""
    return (self.is_patch_in_git or self.is_gerrit_issue)

  @property
  def is_gerrit_issue(self):
    """Returns true iff the properties exist to match a Gerrit issue."""
    if self.gerrit_change:
      return True
    # TODO(tandrii): remove this, once nobody is using buildbot Gerrit Poller.
    return ('event.patchSet.ref' in self.m.properties and
            'event.change.url' in self.m.properties and
            'event.change.id' in self.m.properties)

  @property
  def is_patch_in_git(self):
    return (self.m.properties.get('patch_storage') == 'git' and
            self.m.properties.get('patch_repo_url') and
            self.m.properties.get('patch_ref'))

  def get_files_affected_by_patch(self, patch_root, **kwargs):
    """Returns list of paths to files affected by the patch.

    Args:
      * patch_root: path relative to api.path['root'], usually obtained from
        api.gclient.get_gerrit_patch_root().

    Returned paths will be relative to to patch_root.
    """
    cwd = self.m.context.cwd or self.m.path['start_dir'].join(patch_root)
    with self.m.context(cwd=cwd):
      step_result = self.m.git(
          '-c', 'core.quotePath=false', 'diff', '--cached', '--name-only',
          name='git diff to analyze patch',
          stdout=self.m.raw_io.output(),
          step_test_data=lambda:
            self.m.raw_io.test_api.stream_output('foo.cc'),
          **kwargs)
    paths = [self.m.path.join(patch_root, p) for p in
             step_result.stdout.split()]
    if self.m.platform.is_win:
      # Looks like "analyze" wants POSIX slashes even on Windows (since git
      # uses that format even on Windows).
      paths = [path.replace('\\', '/') for path in paths]
    step_result.presentation.logs['files'] = paths
    return paths

  def set_subproject_tag(self, subproject_tag):
    """Adds a subproject tag to the build.

    This can be used to distinguish between builds that execute different steps
    depending on what was patched, e.g. blink vs. pure chromium patches.
    """
    assert self.is_tryserver

    step_result = self.m.step('TRYJOB SET SUBPROJECT_TAG', cmd=None)
    step_result.presentation.properties['subproject_tag'] = subproject_tag
    step_result.presentation.step_text = subproject_tag

  def _set_failure_type(self, failure_type):
    if not self.is_tryserver:
      return

    # TODO(iannucci): add API to set properties regardless of the current step.
    step_result = self.m.step('TRYJOB FAILURE', cmd=None)
    step_result.presentation.properties['failure_type'] = failure_type
    step_result.presentation.step_text = failure_type
    step_result.presentation.status = 'FAILURE'

  def set_patch_failure_tryjob_result(self):
    """Mark the tryjob result as failure to apply the patch."""
    self._set_failure_type('PATCH_FAILURE')

  def set_compile_failure_tryjob_result(self):
    """Mark the tryjob result as a compile failure."""
    self._set_failure_type('COMPILE_FAILURE')

  def set_test_failure_tryjob_result(self):
    """Mark the tryjob result as a test failure.

    This means we started running actual tests (not prerequisite steps
    like checkout or compile), and some of these tests have failed.
    """
    self._set_failure_type('TEST_FAILURE')

  def set_invalid_test_results_tryjob_result(self):
    """Mark the tryjob result as having invalid test results.

    This means we run some tests, but the results were not valid
    (e.g. no list of specific test cases that failed, or too many
    tests failing, etc).
    """
    self._set_failure_type('INVALID_TEST_RESULTS')

  def set_test_timeout_tryjob_result(self):
    """Mark the tryjob result as a test timeout.

    This means tests were scheduled but didn't finish executing within the
    timeout.
    """
    self._set_failure_type('TEST_TIMEOUT')

  def set_test_expired_tryjob_result(self):
    """Mark the tryjob result as a test expiration.

    This means a test task expired and was never scheduled, most likely due to
    lack of capacity.
    """
    self._set_failure_type('TEST_EXPIRED')

  def get_footers(self, patch_text=None):
    """Retrieves footers from the patch description.

    footers are machine readable tags embedded in commit messages. See
    git-footers documentation for more information.
    """
    if patch_text is None:
      if self.gerrit_change:
        # TODO: reuse _ensure_gerrit_change_info.
        patch_text = self.m.gerrit.get_change_description(
            'https://%s' % self.gerrit_change.host,
            int(self.gerrit_change.change),
            int(self.gerrit_change.patchset))

    result = self.m.python(
        'parse description', self.repo_resource('git_footers.py'),
        args=['--json', self.m.json.output()],
        stdin=self.m.raw_io.input(data=patch_text))
    return result.json.output

  def get_footer(self, tag, patch_text=None):
    """Gets a specific tag from a CL description"""
    return self.get_footers(patch_text).get(tag, [])

  def normalize_footer_name(self, footer):
    return '-'.join([ word.title() for word in footer.strip().split('-') ])
