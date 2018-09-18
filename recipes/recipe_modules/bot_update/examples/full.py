# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

DEPS = [
  'bot_update',
  'gclient',
  'gerrit',
  'tryserver',
  'recipe_engine/buildbucket',
  'recipe_engine/json',
  'recipe_engine/path',
  'recipe_engine/platform',
  'recipe_engine/properties',
  'recipe_engine/runtime',
]

def RunSteps(api):
  api.gclient.use_mirror = True
  commit = api.buildbucket.build.input.gitiles_commit

  src_cfg = api.gclient.make_config(CACHE_DIR='[GIT_CACHE]')
  soln = src_cfg.solutions.add()
  soln.name = 'src'
  soln.url = 'https://chromium.googlesource.com/chromium/src.git'
  soln.revision = commit.id or commit.ref or None
  api.gclient.c = src_cfg
  api.gclient.c.revisions.update(api.properties.get('revisions', {}))
  if api.properties.get('deprecated_got_revision_mapping'):
    api.gclient.c.got_revision_mapping['src'] = 'got_cr_revision'
  else:
    api.gclient.c.got_revision_reverse_mapping['got_cr_revision'] = 'src'
    api.gclient.c.got_revision_reverse_mapping['got_revision'] = 'src'
    api.gclient.c.got_revision_reverse_mapping['got_v8_revision'] = 'src/v8'
    api.gclient.c.got_revision_reverse_mapping['got_angle_revision'] = (
        'src/third_party/angle')
  api.gclient.c.patch_projects['v8'] = ('src/v8', 'HEAD')
  api.gclient.c.patch_projects['v8/v8'] = ('src/v8', 'HEAD')
  api.gclient.c.patch_projects['angle/angle'] = ('src/third_party/angle',
                                                 'HEAD')
  api.gclient.c.patch_projects['webrtc'] = ('src/third_party/webrtc', 'HEAD')
  api.gclient.c.repo_path_map.update({
      'https://chromium.googlesource.com/angle/angle': (
          'src/third_party/angle', 'HEAD'),
      'https://chromium.googlesource.com/v8/v8': ('src/v8', 'HEAD'),
      'https://webrtc.googlesource.com/src': ('src/third_party/webrtc', 'HEAD'),
  })

  patch = api.properties.get('patch', True)
  clobber = True if api.properties.get('clobber') else False
  with_branch_heads = api.properties.get('with_branch_heads', False)
  with_tags = api.properties.get('with_tags', False)
  refs = api.properties.get('refs', [])
  root_solution_revision = api.properties.get('root_solution_revision')
  suffix = api.properties.get('suffix')
  gerrit_no_reset = True if api.properties.get('gerrit_no_reset') else False
  gerrit_no_rebase_patch_ref = bool(
      api.properties.get('gerrit_no_rebase_patch_ref'))
  manifest_name = api.properties.get('manifest_name')
  patch_refs = api.properties.get('patch_refs')

  bot_update_step = api.bot_update.ensure_checkout(
      patch=patch,
      with_branch_heads=with_branch_heads,
      with_tags=with_tags,
      refs=refs,
      clobber=clobber,
      root_solution_revision=root_solution_revision,
      suffix=suffix,
      gerrit_no_reset=gerrit_no_reset,
      gerrit_no_rebase_patch_ref=gerrit_no_rebase_patch_ref,
      disable_syntax_validation=True,
      manifest_name=manifest_name,
      patch_refs=patch_refs)
  if patch:
    api.bot_update.deapply_patch(bot_update_step)


def GenTests(api):

  def try_build(**kwargs):
    kwargs.setdefault(
        'git_repo', 'https://chromium.googlesource.com/chromium/src')
    return api.buildbucket.try_build('chromium', 'linux', **kwargs)

  def ci_build(**kwargs):
    kwargs.setdefault(
        'git_repo', 'https://chromium.googlesource.com/chromium/src')
    return (
        api.buildbucket.ci_build('chromium', 'linux', **kwargs) +
        api.properties(patch=False)
    )


  yield (
      api.test('basic') +
      ci_build()
  )
  yield (
      api.test('input_commit_with_id_without_repo') +
      api.buildbucket.build(api.buildbucket.build_pb2.Build(
          input={
              'gitiles_commit': {
                  'id': 'a' * 40,
              },
          },
      ))
  )
  yield (
      api.test('unrecognized_commit_repo') +
      ci_build(git_repo='https://unrecognized/repo')
  )
  yield (
      api.test('basic_luci') +
      ci_build() +
      api.runtime(is_experimental=False, is_luci=True)
  )
  yield (
      api.test('with_manifest_name') +
      ci_build() +
      api.properties(manifest_name='checkout')
  )
  yield (
      api.test('basic_with_branch_heads') +
      ci_build() +
      api.properties(
          with_branch_heads=True,
          suffix='with branch heads'
      )
  )
  yield (
      api.test('with_tags') +
      api.properties(with_tags=True)
  )
  yield (
      api.test('deprecated_got_revision_mapping') +
      try_build() +
      api.properties(deprecated_got_revision_mapping=True)
  )
  yield (
      api.test('refs') +
      api.properties(refs=['+refs/change/1/2/333'])
  )
  yield (
      api.test('tryjob_fail') +
      try_build() +
      api.step_data('bot_update', api.json.invalid(None), retcode=1)
  )
  yield (
      api.test('tryjob_fail_patch') +
      try_build() +
      api.properties(fail_patch='apply') +
      api.step_data('bot_update', retcode=88)
  )
  yield (
      api.test('tryjob_fail_patch_download') +
      try_build() +
      api.properties(fail_patch='download') +
      api.step_data('bot_update', retcode=87)
  )
  yield (
      api.test('clobber') +
      api.properties(clobber=1)
  )
  yield (
      api.test('reset_root_solution_revision') +
      api.properties(root_solution_revision='revision')
  )
  yield (
      api.test('gerrit_no_reset') +
      api.properties(gerrit_no_reset=1)
  )
  yield (
      api.test('gerrit_no_rebase_patch_ref') +
      api.properties(gerrit_no_rebase_patch_ref=True)
  )
  yield (
      api.test('tryjob_v8') +
      try_build(git_repo='https://chromium.googlesource.com/v8/v8') +
       api.properties(revisions={'src/v8': 'abc'})
  )
  yield (
      api.test('tryjob_v8_head_by_default') +
      try_build(git_repo='https://chromium.googlesource.com/v8/v8')
  )
  yield (
      api.test('tryjob_gerrit_angle') +
      try_build(git_repo='https://chromium.googlesource.com/angle/angle')
  )
  yield (
      api.test('no_apply_patch_on_gclient') +
      try_build(git_repo='https://chromium.googlesource.com/angle/angle') +
      api.bot_update.properties(apply_patch_on_gclient=False)
  )
  yield (
      api.test('tryjob_gerrit_v8_feature_branch') +
      try_build(git_repo='https://chromium.googlesource.com/v8/v8') +
      api.tryserver.gerrit_change_target_ref('refs/heads/experimental/feature')
  )
  yield (
      api.test('tryjob_gerrit_feature_branch') +
      try_build() +
      api.tryserver.gerrit_change_target_ref('refs/heads/experimental/feature')
  )
  yield (
      api.test('tryjob_gerrit_branch_heads') +
      try_build() +
      api.tryserver.gerrit_change_target_ref('refs/branch-heads/67')
  )
  yield (
      api.test('tryjob_gerrit_webrtc') +
      try_build(git_repo='https://webrtc.googlesource.com/src')
  )
  yield (
      api.test('multiple_patch_refs') +
      api.properties(
          patch_refs=[
              ('https://chromium.googlesource.com/chromium/src@'
               'refs/changes/12/34/5'),
              'https://chromium.googlesource.com/v8/v8@refs/changes/124/45/6',
          ],
      )
  )
