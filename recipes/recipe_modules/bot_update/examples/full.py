# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

DEPS = [
  'bot_update',
  'gclient',
  'gerrit',
  'recipe_engine/json',
  'recipe_engine/path',
  'recipe_engine/platform',
  'recipe_engine/properties',
  'recipe_engine/runtime',
]

def RunSteps(api):
  api.gclient.use_mirror = True

  src_cfg = api.gclient.make_config(CACHE_DIR='[GIT_CACHE]')
  soln = src_cfg.solutions.add()
  soln.name = 'src'
  soln.url = 'https://chromium.googlesource.com/chromium/src.git'
  soln.revision = api.properties.get('revision')
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
  api.gclient.c.repo_path_map['https://webrtc.googlesource.com/src'] = (
      'src/third_party/webrtc', 'HEAD')
  api.gclient.c.patch_projects['webrtc'] = ('src/third_party/webrtc', 'HEAD')

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
  yield api.test('basic') + api.properties(
      patch=False,
      revision='abc'
  )
  yield api.test('basic_luci') + api.properties(
      patch=False,
      revision='abc'
  ) + api.runtime(is_experimental=False, is_luci=True)
  yield api.test('with_manifest_name_no_patch') + api.properties(
      manifest_name='checkout',
      patch=False
  )
  yield api.test('with_manifest_name') + api.properties(
      manifest_name='checkout'
  )
  yield api.test('buildbot') + api.properties(
      path_config='buildbot',
      patch=False,
      revision='abc'
  )
  yield api.test('basic_with_branch_heads') + api.properties(
      with_branch_heads=True,
      suffix='with branch heads'
  )
  yield api.test('with_tags') + api.properties(with_tags=True)
  yield api.test('tryjob') + api.properties(
      issue=12345,
      patchset=654321,
      rietveld='https://rietveld.example.com/',
  )
  yield api.test('tryjob_empty_revision') + api.properties(
      issue=12345,
      patchset=654321,
      rietveld='https://rietveld.example.com/',
      revisions={'src': ''},
  )
  yield api.test('deprecated_got_revision_mapping') + api.properties(
      deprecated_got_revision_mapping=True,
      issue=12345,
      patchset=654321,
      rietveld='https://rietveld.example.com/',
  )
  yield api.test('trychange') + api.properties(
      refs=['+refs/change/1/2/333'],
  )
  yield api.test('tryjob_fail') + api.properties(
      issue=12345,
      patchset=654321,
      rietveld='https://rietveld.example.com/',
  ) + api.step_data('bot_update', api.json.invalid(None), retcode=1)
  yield api.test('tryjob_fail_patch') + api.properties(
      issue=12345,
      patchset=654321,
      rietveld='https://rietveld.example.com/',
      fail_patch='apply',
  ) + api.step_data('bot_update', retcode=88)
  yield api.test('tryjob_fail_patch_download') + api.properties(
      issue=12345,
      patchset=654321,
      rietveld='https://rietveld.example.com/',
      fail_patch='download'
  ) + api.step_data('bot_update', retcode=87)
  yield api.test('clobber') + api.properties(
      clobber=1
  )
  yield api.test('reset_root_solution_revision') + api.properties(
      root_solution_revision='revision',
  )
  yield api.test('gerrit_no_reset') + api.properties(
      gerrit_no_reset=1
  )
  yield api.test('gerrit_no_rebase_patch_ref') + api.properties(
      gerrit_no_rebase_patch_ref=True
  )
  yield api.test('tryjob_v8') + api.properties(
      issue=12345,
      patchset=654321,
      rietveld='https://rietveld.example.com/',
      patch_project='v8/v8',
      revisions={'src/v8': 'abc'}
  )
  yield api.test('tryjob_v8_head_by_default') + api.properties.tryserver(
      patch_project='v8/v8',
  )
  yield api.test('tryjob_gerrit_angle') + api.properties.tryserver(
      patch_project='angle/angle',
      patch_issue=91827,
      patch_set=1,
  )
  yield api.test('no_apply_patch_on_gclient') + api.properties.tryserver(
      patch_project='angle/angle',
      patch_issue=91827,
      patch_set=1,
  ) + api.bot_update.properties(
      apply_patch_on_gclient=False,
  )
  yield api.test('tryjob_gerrit_v8') + api.properties.tryserver(
      patch_project='v8/v8',
      patch_issue=91827,
      patch_set=1,
  )
  yield api.test('tryjob_gerrit_v8_feature_branch') + api.properties.tryserver(
      patch_project='v8/v8',
      patch_issue=91827,
      patch_set=1,
  ) + api.step_data(
      'gerrit get change info for '
      'https://chromium-review.googlesource.com/c/91827/1',
      api.gerrit.get_one_change_response_data(
          branch='experimental/feature',
          project='v8/v8',
          o_params=['DOWNLOAD_COMMANDS'],
      ),
  )
  yield api.test('tryjob_gerrit_feature_branch') + api.properties.tryserver(
      buildername='feature_rel',
      patch_project='chromium/src',
      patch_issue=91827,
      patch_set=1,
  ) + api.step_data(
      'gerrit get change info for '
      'https://chromium-review.googlesource.com/c/91827/1',
      api.gerrit.get_one_change_response_data(
          branch='experimental/feature',
          o_params=['DOWNLOAD_COMMANDS'],
      ),
  )
  yield api.test('tryjob_gerrit_branch_heads') + api.properties.tryserver(
      patch_project='chromium/src',
      patch_issue=91827,
      patch_set=1,
  ) + api.step_data(
      'gerrit get change info for '
      'https://chromium-review.googlesource.com/c/91827/1',
      api.gerrit.get_one_change_response_data(
          branch='refs/branch-heads/67',
          o_params=['DOWNLOAD_COMMANDS'],
      ),
  )
  yield api.test('tryjob_gerrit_webrtc') + api.properties.tryserver(
      patch_project='src',
      git_url='https://webrtc.googlesource.com/src',
      patch_issue=91827,
      patch_set=1,
  ) + api.step_data(
     'gerrit get change info for '
     'https://webrtc-review.googlesource.com/c/91827/1',
      api.gerrit.get_one_change_response_data(
          project='src',
          host='https://webrtc.googlesource.com/src',
          o_params=['DOWNLOAD_COMMANDS'],
      ),
  )
  yield api.test('multiple_patch_refs') + api.properties(
      patch=True,
      patch_refs=[
          {'host': 'https://chromium-review.googlesource.com',
           'number': 1234,
           'patchset': 5},
          'https://webrtc.googlesource.com/src@refs/changes/78/5678/6',
      ],
  ) + api.step_data(
      'gerrit get change info for '
      'https://chromium-review.googlesource.com/c/1234/5',
      api.gerrit.get_one_change_response_data(
          patchset=5,
          o_params=['DOWNLOAD_COMMANDS'],
      ),
  )
