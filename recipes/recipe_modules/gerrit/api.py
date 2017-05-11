# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from recipe_engine import recipe_api

class GerritApi(recipe_api.RecipeApi):
  """Module for interact with gerrit endpoints"""

  def __call__(self, name, cmd, infra_step=True, **kwargs):
    """Wrapper for easy calling of gerrit_utils steps."""
    assert isinstance(cmd, (list, tuple))
    prefix = 'gerrit '

    env = self.m.context.env
    env.setdefault('PATH', '%(PATH)s')
    env['PATH'] = self.m.path.pathsep.join([
        env['PATH'], str(self._module.PACKAGE_REPO_ROOT)])

    with self.m.context(env=env):
      return self.m.python(prefix + name,
                           self.package_repo_resource('gerrit_client.py'),
                           cmd,
                           infra_step=infra_step,
                           **kwargs)

  def create_gerrit_branch(self, host, project, branch, commit, **kwargs):
    """
    Create a new branch from given project and commit

    Returns:
      the ref of the branch created
    """
    args = [
        'branch',
        '--host', host,
        '--project', project,
        '--branch', branch,
        '--commit', commit,
        '--json_file', self.m.json.output()
    ]
    step_name = 'create_gerrit_branch'
    step_result = self(step_name, args, **kwargs)
    ref = step_result.json.output.get('ref')
    return ref

  def get_gerrit_branch(self, host, project, branch, **kwargs):
    """
    Get a branch from given project and commit

    Returns:
      the revision of the branch
    """
    args = [
        'branchinfo',
        '--host', host,
        '--project', project,
        '--branch', branch,
        '--json_file', self.m.json.output()
    ]
    step_name='get_gerrit_branch'
    step_result = self(step_name, args, **kwargs)
    revision = step_result.json.output.get('revision')
    return revision
