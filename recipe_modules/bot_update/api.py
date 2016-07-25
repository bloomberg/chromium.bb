# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.


"""Recipe module to ensure a checkout is consistant on a bot."""

from recipe_engine import recipe_api


# This is just for testing, to indicate if a master is using a Git scheduler
# or not.
SVN_MASTERS = (
    'experimental.svn',
)


class BotUpdateApi(recipe_api.RecipeApi):

  def __init__(self, mastername, buildername, slavename, issue, patchset,
               patch_url, repository, gerrit_ref, rietveld, revision,
               parent_got_revision, deps_revision_overrides, fail_patch,
               *args, **kwargs):
    self._mastername = mastername
    self._buildername = buildername
    self._slavename = slavename
    self._issue = issue
    self._patchset = patchset
    self._patch_url = patch_url
    self._repository = repository
    self._gerrit_ref = gerrit_ref
    self._rietveld = rietveld
    self._revision = revision
    self._parent_got_revision = parent_got_revision
    self._deps_revision_overrides = deps_revision_overrides
    self._fail_patch = fail_patch

    self._last_returned_properties = {}
    super(BotUpdateApi, self).__init__(*args, **kwargs)

  def __call__(self, name, cmd, **kwargs):
    """Wrapper for easy calling of bot_update."""
    assert isinstance(cmd, (list, tuple))
    bot_update_path = self.resource('bot_update.py')
    kwargs.setdefault('infra_step', True)
    kwargs.setdefault('env', {})
    kwargs['env'].setdefault('PATH', '%(PATH)s')
    kwargs['env']['PATH'] = self.m.path.pathsep.join([
        kwargs['env']['PATH'], str(self._module.PACKAGE_REPO_ROOT)])
    return self.m.python(name, bot_update_path, cmd, **kwargs)

  @property
  def last_returned_properties(self):
      return self._last_returned_properties

  def ensure_checkout(self, gclient_config=None, suffix=None,
                      patch=True, update_presentation=True,
                      force=False, patch_root=None, no_shallow=False,
                      with_branch_heads=False, refs=None,
                      patch_oauth2=False, use_site_config_creds=True,
                      output_manifest=True, clobber=False,
                      root_solution_revision=None, rietveld=None, issue=None,
                      patchset=None, gerrit_no_reset=False, **kwargs):
    """
    Args:
      use_site_config_creds: If the oauth2 credentials are in the buildbot
        site_config. See crbug.com/624212 for more information.
      gclient_config: The gclient configuration to use when running bot_update.
        If omitted, the current gclient configuration is used.
      rietveld: The rietveld server to use. If omitted, will infer from
        the 'rietveld' property.
      issue: The rietveld issue number to use. If omitted, will infer from
        the 'issue' property.
      patchset: The rietveld issue patchset to use. If omitted, will infer from
        the 'patchset' property.
    """
    refs = refs or []
    # We can re-use the gclient spec from the gclient module, since all the
    # data bot_update needs is already configured into the gclient spec.
    cfg = gclient_config or self.m.gclient.c
    assert cfg is not None, (
        'missing gclient_config or forgot api.gclient.set_config(...) before?')

    # Used by bot_update to determine if we want to run or not.
    master = self._mastername
    builder = self._buildername
    slave = self._slavename

    # Construct our bot_update command.  This basically be inclusive of
    # everything required for bot_update to know:
    root = patch_root
    if root is None:
      root = self.m.gclient.calculate_patch_root(
          self.m.properties.get('patch_project'), cfg)

    if patch:
      issue = issue or self._issue
      patchset = patchset or self._patchset
      patch_url = self._patch_url
      gerrit_repo = self._repository
      gerrit_ref = self._gerrit_ref
    else:
      # The trybot recipe sometimes wants to de-apply the patch. In which case
      # we pretend the issue/patchset/patch_url never existed.
      issue = patchset = patch_url = email_file = key_file = None
      gerrit_repo = gerrit_ref = None

    # Issue and patchset must come together.
    if issue:
      assert patchset
    if patchset:
      assert issue
    if patch_url:
      # If patch_url is present, bot_update will actually ignore issue/ps.
      issue = patchset = None

    # The gerrit_ref and gerrit_repo must be together or not at all.  If one is
    # missing, clear both of them.
    if not gerrit_ref or not gerrit_repo:
      gerrit_repo = gerrit_ref = None
    assert (gerrit_ref != None) == (gerrit_repo != None)

    # Point to the oauth2 auth files if specified.
    # These paths are where the bots put their credential files.
    if patch_oauth2:
      # TODO(martiniss): remove this hack :(. crbug.com/624212
      if use_site_config_creds:
        email_file = self.m.path['build'].join(
            'site_config', '.rietveld_client_email')
        key_file = self.m.path['build'].join(
            'site_config', '.rietveld_secret_key')
      else: #pragma: no cover
        #TODO(martiniss): make this use path.join, so it works on windows
        email_file = '/creds/rietveld/client_email'
        key_file = '/creds/rietveld/secret_key'
    else:
      email_file = key_file = None

    # Allow patch_project's revision if necessary.
    # This is important for projects which are checked out as DEPS of the
    # gclient solution.
    self.m.gclient.set_patch_project_revision(
        self.m.properties.get('patch_project'), cfg)

    rev_map = cfg.got_revision_mapping.as_jsonish()

    flags = [
        # 1. Do we want to run? (master/builder/slave).
        ['--master', master],
        ['--builder', builder],
        ['--slave', slave],

        # 2. What do we want to check out (spec/root/rev/rev_map).
        ['--spec', self.m.gclient.config_to_pythonish(cfg)],
        ['--root', root],
        ['--revision_mapping_file', self.m.json.input(rev_map)],
        ['--git-cache-dir', cfg.cache_dir],

        # 3. How to find the patch, if any (issue/patchset/patch_url).
        ['--issue', issue],
        ['--patchset', patchset],
        ['--patch_url', patch_url],
        ['--rietveld_server', rietveld or self._rietveld],
        ['--gerrit_repo', gerrit_repo],
        ['--gerrit_ref', gerrit_ref],
        ['--apply_issue_email_file', email_file],
        ['--apply_issue_key_file', key_file],

        # 4. Hookups to JSON output back into recipes.
        ['--output_json', self.m.json.output()],]


    # Collect all fixed revisions to simulate them in the json output.
    # Fixed revision are the explicit input revisions of bot_update.py, i.e.
    # every command line parameter "--revision name@value".
    fixed_revisions = {}

    revisions = {}
    for solution in cfg.solutions:
      if solution.revision:
        revisions[solution.name] = solution.revision
      elif solution == cfg.solutions[0]:
        revisions[solution.name] = (
            self._parent_got_revision or
            self._revision or
            'HEAD')
    if self.m.gclient.c and self.m.gclient.c.revisions:
      revisions.update(self.m.gclient.c.revisions)
    if cfg.solutions and root_solution_revision:
      revisions[cfg.solutions[0].name] = root_solution_revision
    # Allow for overrides required to bisect into rolls.
    revisions.update(self._deps_revision_overrides)
    for name, revision in sorted(revisions.items()):
      fixed_revision = self.m.gclient.resolve_revision(revision)
      if fixed_revision:
        fixed_revisions[name] = fixed_revision
        flags.append(['--revision', '%s@%s' % (name, fixed_revision)])

    # Add extra fetch refspecs.
    for ref in refs:
      flags.append(['--refs', ref])

    # Filter out flags that are None.
    cmd = [item for flag_set in flags
           for item in flag_set if flag_set[1] is not None]

    if clobber:
      cmd.append('--clobber')
    if force:
      cmd.append('--force')
    if no_shallow:
      cmd.append('--no_shallow')
    if output_manifest:
      cmd.append('--output_manifest')
    if with_branch_heads or cfg.with_branch_heads:
      cmd.append('--with_branch_heads')
    if gerrit_no_reset:
      cmd.append('--gerrit_no_reset')

    # Inject Json output for testing.
    git_mode = self._mastername not in SVN_MASTERS
    first_sln = cfg.solutions[0].name
    step_test_data = lambda: self.test_api.output_json(
        master, builder, slave, root, first_sln, rev_map, git_mode, force,
        self._fail_patch,
        output_manifest=output_manifest, fixed_revisions=fixed_revisions)

    # Add suffixes to the step name, if specified.
    name = 'bot_update'
    if not patch:
      name += ' (without patch)'
    if suffix:
      name += ' - %s' % suffix

    # Ah hah! Now that everything is in place, lets run bot_update!
    step_result = None
    try:
      # 87 and 88 are the 'patch failure' codes for patch download and patch
      # apply, respectively. We don't actually use the error codes, and instead
      # rely on emitted json to determine cause of failure.
      step_result = self(name, cmd, step_test_data=step_test_data,
           ok_ret=(0, 87, 88), **kwargs)
    except self.m.step.StepFailure as f:
      step_result = f.result
      raise
    finally:
      if step_result:
        self._last_returned_properties = step_result.json.output.get(
            'properties', {})

        if update_presentation:
          # Set properties such as got_revision.
          for prop_name, prop_value in (
              self.last_returned_properties.iteritems()):
            step_result.presentation.properties[prop_name] = prop_value
        # Add helpful step description in the step UI.
        if 'step_text' in step_result.json.output:
          step_text = step_result.json.output['step_text']
          step_result.presentation.step_text = step_text
        # Add log line output.
        if 'log_lines' in step_result.json.output:
          for log_name, log_lines in step_result.json.output['log_lines']:
            step_result.presentation.logs[log_name] = log_lines.splitlines()

        # Set the "checkout" path for the main solution.
        # This is used by the Chromium module to figure out where to look for
        # the checkout.
        # If there is a patch failure, emit another step that said things
        # failed.
        if step_result.json.output.get('patch_failure'):
          return_code = step_result.json.output.get('patch_apply_return_code')
          if return_code == 3:
            # This is download failure, hence an infra failure.
            # Sadly, python.failing_step doesn't support kwargs.
            self.m.python.inline(
                'Patch failure',
                ('import sys;'
                 'print "Patch download failed. See bot_update step for'
                 ' details";sys.exit(1)'),
                infra_step=True,
                step_test_data=lambda: self.m.raw_io.test_api.output(
                  'Patch download failed. See bot_update step for details',
                  retcode=1)
                )
          else:
            # This is actual patch failure.
            self.m.tryserver.set_patch_failure_tryjob_result()
            self.m.python.failing_step(
                'Patch failure', 'Check the bot_update step for details')

        # bot_update actually just sets root to be the folder name of the
        # first solution.
        if step_result.json.output['did_run']:
          co_root = step_result.json.output['root']
          cwd = kwargs.get('cwd', self.m.path['slave_build'])
          if 'checkout' not in self.m.path:
            self.m.path['checkout'] = cwd.join(*co_root.split(self.m.path.sep))

    return step_result
