# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Recipe module to ensure a checkout is consistent on a bot."""

from recipe_engine import recipe_api


class BotUpdateApi(recipe_api.RecipeApi):

  def __init__(self, properties, deps_revision_overrides, fail_patch, *args,
               **kwargs):
    self._apply_patch_on_gclient = properties.get(
        'apply_patch_on_gclient', True)
    self._deps_revision_overrides = deps_revision_overrides
    self._fail_patch = fail_patch

    self._last_returned_properties = {}
    super(BotUpdateApi, self).__init__(*args, **kwargs)

  def initialize(self):
    assert len(self.m.buildbucket.build.input.gerrit_changes) <= 1, (
        'bot_update does not support more than one '
        'buildbucket.build.input.gerrit_changes')

  def __call__(self, name, cmd, **kwargs):
    """Wrapper for easy calling of bot_update."""
    assert isinstance(cmd, (list, tuple))
    bot_update_path = self.resource('bot_update.py')
    kwargs.setdefault('infra_step', True)

    with self.m.depot_tools.on_path():
      return self.m.python(name, bot_update_path, cmd, **kwargs)

  @property
  def last_returned_properties(self):
      return self._last_returned_properties

  def _get_commit_repo_path(self, commit, gclient_config):
    """Returns local path to the repo that the commit is associated with.

    The commit must be a self.m.buildbucket.common_pb2.GitilesCommit.
    If commit does not specify any repo, returns name of the first solution.

    Raises an InfraFailure if the commit specifies a repo unexpected by gclient.
    """
    assert gclient_config.solutions, 'gclient_config.solutions is empty'

    # if repo is not specified, choose the first solution.
    if not (commit.host and commit.project):
      return gclient_config.solutions[0].name
    assert commit.host and commit.project

    repo_url = self.m.gitiles.unparse_repo_url(commit.host, commit.project)
    repo_path = self.m.gclient.get_repo_path(
        repo_url, gclient_config=gclient_config)
    if not repo_path:
      raise self.m.step.InfraFailure(
          'invalid (host, project) pair in '
          'buildbucket.build.input.gitiles_commit: '
          '(%r, %r) does not match any of configured gclient solutions '
          'and not present in gclient_config.repo_path_map' % (
              commit.host, commit.project))

    return repo_path

  def ensure_checkout(self, gclient_config=None, suffix=None,
                      patch=True, update_presentation=True,
                      patch_root=None,
                      with_branch_heads=False, with_tags=False, refs=None,
                      patch_oauth2=None, oauth2_json=None,
                      use_site_config_creds=None, clobber=False,
                      root_solution_revision=None, rietveld=None, issue=None,
                      patchset=None, gerrit_no_reset=False,
                      gerrit_no_rebase_patch_ref=False,
                      disable_syntax_validation=False, manifest_name=None,
                      patch_refs=None, ignore_input_commit=False,
                      **kwargs):
    """
    Args:
      gclient_config: The gclient configuration to use when running bot_update.
        If omitted, the current gclient configuration is used.
      disable_syntax_validation: (legacy) Disables syntax validation for DEPS.
        Needed as migration paths for recipes dealing with older revisions,
        such as bisect.
      manifest_name: The name of the manifest to upload to LogDog.  This must
        be unique for the whole build.
    """
    assert use_site_config_creds is None, "use_site_config_creds is deprecated"
    assert rietveld is None, "rietveld is deprecated"
    assert issue is None, "issue is deprecated"
    assert patchset is None, "patchset is deprecated"
    assert patch_oauth2 is None, "patch_oauth2 is deprecated"
    assert oauth2_json is None, "oauth2_json is deprecated"

    refs = refs or []
    # We can re-use the gclient spec from the gclient module, since all the
    # data bot_update needs is already configured into the gclient spec.
    cfg = gclient_config or self.m.gclient.c
    assert cfg is not None, (
        'missing gclient_config or forgot api.gclient.set_config(...) before?')

    # Construct our bot_update command.  This basically be inclusive of
    # everything required for bot_update to know:
    root = patch_root
    if root is None:
      # TODO(nodir): use m.gclient.get_repo_path instead.
      root = self.m.gclient.calculate_patch_root(
          self.m.properties.get('patch_project'), cfg,
          self.m.tryserver.gerrit_change_repo_url)

    # Allow patch_project's revision if necessary.
    # This is important for projects which are checked out as DEPS of the
    # gclient solution.
    self.m.gclient.set_patch_project_revision(
        self.m.properties.get('patch_project'), cfg)

    reverse_rev_map = self.m.gclient.got_revision_reverse_mapping(cfg)

    flags = [
        # What do we want to check out (spec/root/rev/reverse_rev_map).
        ['--spec-path', self.m.raw_io.input(
            self.m.gclient.config_to_pythonish(cfg))],
        ['--patch_root', root],
        ['--revision_mapping_file', self.m.json.input(reverse_rev_map)],
        ['--git-cache-dir', cfg.cache_dir],
        ['--cleanup-dir', self.m.path['cleanup'].join('bot_update')],

        # Hookups to JSON output back into recipes.
        ['--output_json', self.m.json.output()],
    ]

    # How to find the patch, if any
    if patch:
      repo_url = self.m.tryserver.gerrit_change_repo_url
      fetch_ref = self.m.tryserver.gerrit_change_fetch_ref
      if repo_url and fetch_ref:
        flags.append(['--patch_ref', '%s@%s' % (repo_url, fetch_ref)])
      if patch_refs:
        flags.extend(
            ['--patch_ref', patch_ref]
            for patch_ref in patch_refs)

    # Compute requested revisions.
    revisions = {}
    for solution in cfg.solutions:
      if solution.revision:
        revisions[solution.name] = solution.revision

    # HACK: ensure_checkout API must be redesigned so that we don't pass such
    # parameters. Existing semantics is too opiniated.
    if not ignore_input_commit:
      # Apply input gitiles_commit, if any.
      input_commit = self.m.buildbucket.build.input.gitiles_commit
      if input_commit.id or input_commit.ref:
        repo_path = self._get_commit_repo_path(input_commit, cfg)
        # Note: this is not entirely correct. build.input.gitiles_commit
        # definition says "The Gitiles commit to run against.".
        # However, here we ignore it if the config specified a revision.
        # This is necessary because existing builders rely on this behavior,
        # e.g. they want to force refs/heads/master at the config level.
        revisions[repo_path] = (
            revisions.get(repo_path) or input_commit.id or input_commit.ref)

    # Guarantee that first solution has a revision.
    # TODO(machenbach): We should explicitly pass HEAD for ALL solutions
    # that don't specify anything else.
    first_sol = cfg.solutions[0].name
    revisions[first_sol] = revisions.get(first_sol) or 'HEAD'

    if cfg.revisions:
      # Only update with non-empty values. Some recipe might otherwise
      # overwrite the HEAD default with an empty string.
      revisions.update(
          (k, v) for k, v in cfg.revisions.iteritems() if v)
    if cfg.solutions and root_solution_revision:
      revisions[first_sol] = root_solution_revision
    # Allow for overrides required to bisect into rolls.
    revisions.update(self._deps_revision_overrides)

    # Compute command-line parameters for requested revisions.
    # Also collect all fixed revisions to simulate them in the json output.
    # Fixed revision are the explicit input revisions of bot_update.py, i.e.
    # every command line parameter "--revision name@value".
    fixed_revisions = {}
    for name, revision in sorted(revisions.items()):
      fixed_revision = self.m.gclient.resolve_revision(revision)
      if fixed_revision:
        fixed_revisions[name] = fixed_revision
        if fixed_revision.upper() == 'HEAD':
          # Sync to correct destination branch if HEAD was specified.
          fixed_revision = self._destination_branch(cfg, name)
        # If we're syncing to a ref, we want to make sure it exists before
        # trying to check it out.
        if (fixed_revision.startswith('refs/') and
            # TODO(crbug.com/874501): fetching additional refs is currently
            # only supported for the root solution. We should investigate
            # supporting it for other dependencies.
            cfg.solutions and
            cfg.solutions[0].name == name):
          # Handle the "ref:revision" syntax, e.g.
          # refs/branch-heads/4.2:deadbeef
          refs.append(fixed_revision.split(':')[0])
        flags.append(['--revision', '%s@%s' % (name, fixed_revision)])

    for ref in refs:
      assert not ref.startswith('refs/remotes/'), (
          'The "refs/remotes/*" syntax is not supported.\n'
          'The "remotes" syntax is dependent on the way the local repo is '
          'configured, and while there are defaults that can often be '
          'assumed, there is no guarantee the mapping will always be done in '
          'a particular way.')

    # Add extra fetch refspecs.
    for ref in refs:
      flags.append(['--refs', ref])

    # Filter out flags that are None.
    cmd = [item for flag_set in flags
           for item in flag_set if flag_set[1] is not None]

    if clobber:
      cmd.append('--clobber')
    if with_branch_heads or cfg.with_branch_heads:
      cmd.append('--with_branch_heads')
    if with_tags or cfg.with_tags:
      cmd.append('--with_tags')
    if gerrit_no_reset:
      cmd.append('--gerrit_no_reset')
    if gerrit_no_rebase_patch_ref:
      cmd.append('--gerrit_no_rebase_patch_ref')
    if disable_syntax_validation or cfg.disable_syntax_validation:
      cmd.append('--disable-syntax-validation')
    if not self._apply_patch_on_gclient:
      cmd.append('--no-apply-patch-on-gclient')

    # Inject Json output for testing.
    first_sln = cfg.solutions[0].name
    step_test_data = lambda: self.test_api.output_json(
        root, first_sln, reverse_rev_map, self._fail_patch,
        fixed_revisions=fixed_revisions)

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
      step_result = self(
           name, cmd, step_test_data=step_test_data,
           ok_ret=(0, 87, 88), **kwargs)
    except self.m.step.StepFailure as f:
      step_result = f.result
      raise
    finally:
      if step_result and step_result.json.output:
        result = step_result.json.output
        self._last_returned_properties = step_result.json.output.get(
            'properties', {})

        if update_presentation:
          # Set properties such as got_revision.
          for prop_name, prop_value in (
              self.last_returned_properties.iteritems()):
            step_result.presentation.properties[prop_name] = prop_value
        # Add helpful step description in the step UI.
        if 'step_text' in result:
          step_text = result['step_text']
          step_result.presentation.step_text = step_text

        # Export the step results as a Source Manifest to LogDog.
        if manifest_name:
          if not patch:
            # The param "patched" is purely cosmetic to mean "if false, this
            # bot_update run exists purely to unpatch an existing patch".
            manifest_name += '_unpatched'
          self.m.source_manifest.set_json_manifest(
              manifest_name, result.get('source_manifest', {}))

        # Set the "checkout" path for the main solution.
        # This is used by the Chromium module to figure out where to look for
        # the checkout.
        # If there is a patch failure, emit another step that said things
        # failed.
        if result.get('patch_failure'):
          return_code = result.get('patch_apply_return_code')
          patch_body = result.get('failed_patch_body')
          try:
            if return_code == 3:
              # This is download failure, hence an infra failure.
              with self.m.context(infra_steps=True):
                self.m.python.failing_step(
                    'Patch failure', 'Git reported a download failure')
            else:
              # This is actual patch failure.
              self.m.tryserver.set_patch_failure_tryjob_result()
              self.m.python.failing_step(
                  'Patch failure', 'See attached log. Try rebasing?')
          except self.m.step.StepFailure as e:
            if patch_body:
              e.result.presentation.logs['patch error'] = (
                  patch_body.splitlines())
            raise e

        # bot_update actually just sets root to be the folder name of the
        # first solution.
        if result['did_run'] and 'checkout' not in self.m.path:
          co_root = result['root']
          cwd = self.m.context.cwd or self.m.path['start_dir']
          self.m.path['checkout'] = cwd.join(*co_root.split(self.m.path.sep))

    return step_result

  def _destination_branch(self, cfg, path):
    """Returns the destination branch of a CL for the matching project
    if available or HEAD otherwise.

    If there's no Gerrit CL associated with the run, returns 'HEAD'.
    Otherwise this queries Gerrit for the correct destination branch, which
    might differ from master.

    Args:
      cfg: The used gclient config.
      path: The DEPS path of the project this prefix is for. E.g. 'src' or
          'src/v8'. The query will only be made for the project that matches
          the CL's project.
    Returns:
        A destination branch as understood by bot_update.py if available
        and if different from master, returns 'HEAD' otherwise.
    """
    # Ignore project paths other than the one belonging to the current CL.
    patch_path = self.m.gclient.get_gerrit_patch_root(gclient_config=cfg)
    if not patch_path or path != patch_path:
      return 'HEAD'

    target_ref = self.m.tryserver.gerrit_change_target_ref
    if target_ref == 'refs/heads/master':
      return 'HEAD'

    # TODO: Remove. Return ref, not branch.
    ret = target_ref
    prefix = 'refs/heads/'
    if ret.startswith(prefix):
      ret = ret[len(prefix):]

    return ret

  def _resolve_fixed_revisions(self, bot_update_json):
    """Set all fixed revisions from the first sync to their respective
    got_X_revision values.

    If on the first sync, a revision was requested to be HEAD, this avoids
    using HEAD potentially resolving to a different revision on the second
    sync. Instead, we sync explicitly to whatever was checked out the first
    time.

    Example (chromium trybot used with v8 patch):

    First sync was called with
    bot_update.py --revision src@abc --revision src/v8@HEAD
    Fixed revisions are: src, src/v8
    Got_revision_mapping: src->got_revision, src/v8->got_v8_revision
    got_revision = abc, got_v8_revision = deadbeef
    Second sync will be called with
    bot_update.py --revision src@abc --revision src/v8@deadbeef

    Example (chromium trybot used with chromium DEPS change, changing v8 from
    "v8_before" to "v8_after"):

    First sync was called with
    bot_update.py --revision src@abc
    Fixed revisions are: src
    Got_revision_mapping: src->got_revision, src/v8->got_v8_revision
    got_revision = abc, got_v8_revision = v8_after
    Second sync will be called with
    bot_update.py --revision src@abc
    When deapplying the patch, v8 will be synced to v8_before.
    """
    for name in bot_update_json.get('fixed_revisions', {}):
      rev_properties = self.get_project_revision_properties(name)
      if (rev_properties and
          bot_update_json['properties'].get(rev_properties[0])):
        self.m.gclient.c.revisions[name] = str(
            bot_update_json['properties'][rev_properties[0]])

  # TODO(machenbach): Replace usages of this method eventually by direct calls
  # to the manifest output.
  def get_project_revision_properties(self, project_name, gclient_config=None):
    """Returns all property names used for storing the checked-out revision of
    a given project.

    Args:
      project_name (str): The name of a checked-out project as deps path, e.g.
          src or src/v8.
      gclient_config: The gclient configuration to use. If omitted, the current
          gclient configuration is used.

    Returns (list of str): All properties that'll hold the checked-out revision
        of the given project. An empty list if no such properties exist.
    """
    cfg = gclient_config or self.m.gclient.c
    # Sort for determinism. We might have several properties for the same
    # project, e.g. got_revision and got_webrtc_revision.
    rev_reverse_map = self.m.gclient.got_revision_reverse_mapping(cfg)
    return sorted(
        prop
        for prop, project in rev_reverse_map.iteritems()
        if project == project_name
    )

  def deapply_patch(self, bot_update_step):
    """Deapplies a patch, taking care of DEPS and solution revisions properly.
    """
    bot_update_json = bot_update_step.json.output
    # We only override first solution here to make sure that we correctly revert
    # changes to DEPS file, which is particularly important for auto-rolls. It
    # is also imporant that we do not assume that corresponding revision is
    # stored in the 'got_revision' as some gclient configs change the default
    # mapping for their own purposes.
    first_solution_name = self.m.gclient.c.solutions[0].name
    rev_property = self.get_project_revision_properties(first_solution_name)[0]
    self.m.gclient.c.revisions[first_solution_name] = str(
        bot_update_json['properties'][rev_property])
    self._resolve_fixed_revisions(bot_update_json)

    self.ensure_checkout(patch=False, update_presentation=False)
