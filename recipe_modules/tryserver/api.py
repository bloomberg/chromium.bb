# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import collections
import contextlib
import hashlib

from recipe_engine import recipe_api


PATCH_STORAGE_RIETVELD = 'rietveld'
PATCH_STORAGE_GIT = 'git'
PATCH_STORAGE_SVN = 'svn'


class TryserverApi(recipe_api.RecipeApi):
  def __init__(self, *args, **kwargs):
    super(TryserverApi, self).__init__(*args, **kwargs)
    self._failure_reasons = []

  @property
  def patch_url(self):
    """Reads patch_url property and corrects it if needed."""
    url = self.m.properties.get('patch_url')
    return url

  @property
  def is_tryserver(self):
    """Returns true iff we can apply_issue or patch."""
    return (self.can_apply_issue or self.is_patch_in_svn or
            self.is_patch_in_git or self.is_gerrit_issue)

  @property
  def can_apply_issue(self):
    """Returns true iff the properties exist to apply_issue from rietveld."""
    return (self.m.properties.get('rietveld')
            and 'issue' in self.m.properties
            and 'patchset' in self.m.properties)

  @property
  def is_gerrit_issue(self):
    """Returns true iff the properties exist to match a Gerrit issue."""
    return ('event.patchSet.ref' in self.m.properties and
            'event.change.url' in self.m.properties and
            'event.change.id' in self.m.properties)

  @property
  def is_patch_in_svn(self):
    """Returns true iff the properties exist to patch from a patch URL."""
    return self.patch_url

  @property
  def is_patch_in_git(self):
    return (self.m.properties.get('patch_storage') == PATCH_STORAGE_GIT and
            self.m.properties.get('patch_repo_url') and
            self.m.properties.get('patch_ref'))

  def _apply_patch_step(self, patch_file=None, patch_content=None, root=None):
    assert not (patch_file and patch_content), (
        'Please only specify either patch_file or patch_content, not both!')
    patch_cmd = [
        'patch',
        '--dir', root or self.m.path['checkout'],
        '--force',
        '--forward',
        '--remove-empty-files',
        '--strip', '0',
    ]
    if patch_file:
      patch_cmd.extend(['--input', patch_file])

    self.m.step('apply patch', patch_cmd,
                      stdin=patch_content)

  def apply_from_svn(self, cwd):
    """Downloads patch from patch_url using svn-export and applies it"""
    # TODO(nodir): accept these properties as parameters
    patch_url = self.patch_url
    root = cwd
    if root is None:
      issue_root = self.m.rietveld.calculate_issue_root()
      root = self.m.path['checkout'].join(issue_root)

    patch_file = self.m.raw_io.output('.diff')
    ext = '.bat' if self.m.platform.is_win else ''
    svn_cmd = ['svn' + ext, 'export', '--force', patch_url, patch_file]

    result = self.m.step('download patch', svn_cmd,
                         step_test_data=self.test_api.patch_content)
    result.presentation.logs['patch.diff'] = (
        result.raw_io.output.split('\n'))

    patch_content = self.m.raw_io.input(result.raw_io.output)
    self._apply_patch_step(patch_content=patch_content, root=root)

  def apply_from_git(self, cwd):
    """Downloads patch from given git repo and ref and applies it"""
    # TODO(nodir): accept these properties as parameters
    patch_repo_url = self.m.properties['patch_repo_url']
    patch_ref = self.m.properties['patch_ref']

    patch_dir = self.m.path.mkdtemp('patch')
    git_setup_py = self.m.path['build'].join('scripts', 'slave', 'git_setup.py')
    git_setup_args = ['--path', patch_dir, '--url', patch_repo_url]
    patch_path = patch_dir.join('patch.diff')

    self.m.python('patch git setup', git_setup_py, git_setup_args)
    self.m.git('fetch', 'origin', patch_ref,
                name='patch fetch', cwd=patch_dir)
    self.m.git('clean', '-f', '-d', '-x',
                name='patch clean', cwd=patch_dir)
    self.m.git('checkout', '-f', 'FETCH_HEAD',
                name='patch git checkout', cwd=patch_dir)
    self._apply_patch_step(patch_file=patch_path, root=cwd)
    self.m.step('remove patch', ['rm', '-rf', patch_dir])

  def determine_patch_storage(self):
    """Determines patch_storage automatically based on properties."""
    storage = self.m.properties.get('patch_storage')
    if storage:
      return storage

    if self.can_apply_issue:
      return PATCH_STORAGE_RIETVELD
    elif self.is_patch_in_svn:
      return PATCH_STORAGE_SVN

  def maybe_apply_issue(self, cwd=None, authentication=None):
    """If we're a trybot, apply a codereview issue.

    Args:
      cwd: If specified, apply the patch from the specified directory.
      authentication: authentication scheme whenever apply_issue.py is called.
        This is only used if the patch comes from Rietveld. Possible values:
        None, 'oauth2' (see also api.rietveld.apply_issue.)
    """
    storage = self.determine_patch_storage()

    if storage == PATCH_STORAGE_RIETVELD:
      return self.m.rietveld.apply_issue(
          self.m.rietveld.calculate_issue_root(),
          authentication=authentication)
    elif storage == PATCH_STORAGE_SVN:
      return self.apply_from_svn(cwd)
    elif storage == PATCH_STORAGE_GIT:
      return self.apply_from_git(cwd)
    else:
      # Since this method is "maybe", we don't raise an Exception.
      pass

  def get_files_affected_by_patch(self, patch_root=None, **kwargs):
    """Returns list of paths to files affected by the patch.

    Argument:
      patch_root: path relative to api.path['root'], usually obtained from
        api.gclient.calculate_patch_root(patch_project)

    Returned paths will be relative to to patch_root.

    TODO(tandrii): remove this doc.
    Unless you use patch_root=None, in which case old behavior is used
    which returns paths relative to checkout aka solution[0].name.
    """
    # patch_root must be set! None is for backwards compataibility and will be
    # removed.
    if patch_root is None:
      return self._old_get_files_affected_by_patch()
    if not kwargs.get('cwd'):
      kwargs['cwd'] = self.m.path['slave_build'].join(patch_root)
    step_result = self.m.git('diff', '--cached', '--name-only',
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


  def _old_get_files_affected_by_patch(self):
    git_diff_kwargs = {}
    issue_root = self.m.rietveld.calculate_issue_root()
    if issue_root:
      git_diff_kwargs['cwd'] = self.m.path['checkout'].join(issue_root)
    step_result = self.m.git('diff', '--cached', '--name-only',
                             name='git diff to analyze patch',
                             stdout=self.m.raw_io.output(),
                             step_test_data=lambda:
                               self.m.raw_io.test_api.stream_output('foo.cc'),
                             **git_diff_kwargs)
    paths = step_result.stdout.split()
    if issue_root:
      paths = [self.m.path.join(issue_root, path) for path in paths]
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

    step_result = self.m.step.active_result
    step_result.presentation.properties['subproject_tag'] = subproject_tag

  def _set_failure_type(self, failure_type):
    if not self.is_tryserver:
      return

    step_result = self.m.step.active_result
    step_result.presentation.properties['failure_type'] = failure_type

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

  def add_failure_reason(self, reason):
    """
    Records a more detailed reason why build is failing.

    The reason can be any JSON-serializable object.
    """
    assert self.m.json.is_serializable(reason)
    self._failure_reasons.append(reason)

  @contextlib.contextmanager
  def set_failure_hash(self):
    """
    Context manager that sets a failure_hash build property on StepFailure.

    This can be used to easily compare whether two builds have failed
    for the same reason. For example, if a patch is bad (breaks something),
    we'd expect it to always break in the same way. Different failures
    for the same patch are usually a sign of flakiness.
    """
    try:
      yield
    except self.m.step.StepFailure as e:
      self.add_failure_reason(e.reason)

      failure_hash = hashlib.sha1()
      failure_hash.update(self.m.json.dumps(self._failure_reasons))

      step_result = self.m.step.active_result
      step_result.presentation.properties['failure_hash'] = \
          failure_hash.hexdigest()

      raise

  def get_footers(self, patch_text=None):
    """Retrieves footers from the patch description.

    footers are machine readable tags embedded in commit messages. See
    git-footers documentation for more information.
    """
    if patch_text is None:
      codereview = None
      if not self.can_apply_issue: #pragma: no cover
        raise recipe_api.StepFailure("Cannot get tags from gerrit yet.")
      else:
        codereview = 'rietveld'
        patch = (
            self.m.properties['rietveld'].strip('/') + '/' +
            str(self.m.properties['issue']))

      patch_text = self.m.git_cl.get_description(
          patch=patch, codereview=codereview).stdout

    result = self.m.python(
        'parse description', self.package_repo_resource('git_footers.py'),
        args=['--json', self.m.json.output()],
        stdin=self.m.raw_io.input(data=patch_text))
    return result.json.output

  def get_footer(self, tag, patch_text=None):
    """Gets a specific tag from a CL description"""
    return self.get_footers(patch_text).get(tag, [])

