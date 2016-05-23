# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

DEPS = [
  'recipe_engine/json',
  'recipe_engine/raw_io',
  'recipe_engine/path',
  'recipe_engine/platform',
  'recipe_engine/properties',
  'recipe_engine/python',
  'recipe_engine/step',
  'tryserver',
]


def RunSteps(api):
  api.path['checkout'] = api.path['slave_build']
  if api.properties.get('patch_text'):
    api.step('patch_text test', [
        'echo', str(api.tryserver.get_footers(api.properties['patch_text']))])
    api.step('patch_text test', [
        'echo', str(api.tryserver.get_footer(
            'Foo', api.properties['patch_text']))])
    return

  api.tryserver.maybe_apply_issue()
  if api.tryserver.can_apply_issue:
    api.tryserver.get_footers()
  api.tryserver.get_files_affected_by_patch(
      api.properties.get('test_patch_root'))

  if api.tryserver.is_tryserver:
    api.tryserver.set_subproject_tag('v8')

  api.tryserver.set_patch_failure_tryjob_result()
  api.tryserver.set_compile_failure_tryjob_result()
  api.tryserver.set_test_failure_tryjob_result()
  api.tryserver.set_invalid_test_results_tryjob_result()

  with api.tryserver.set_failure_hash():
    api.python.failing_step('fail', 'foo')


def GenTests(api):
  description_step = api.override_step_data(
      'git_cl description', stdout=api.raw_io.output('foobar'))
  yield (api.test('with_svn_patch') +
         api.properties(patch_url='svn://checkout.url'))

  yield (api.test('with_git_patch') +
         api.properties(
              patch_storage='git',
              patch_project='v8',
              patch_repo_url='http://patch.url/',
              patch_ref='johndoe#123.diff'))

  yield (api.test('with_rietveld_patch') +
         api.properties.tryserver() +
         description_step)

  yield (api.test('with_wrong_patch') + api.platform('win', 32))

  yield (api.test('with_rietveld_patch_new') +
         api.properties.tryserver(test_patch_root='sub/project') +
         description_step)

  yield (api.test('with_wrong_patch_new') + api.platform('win', 32) +
         api.properties(test_patch_root='sub\\project'))

  yield (api.test('basic_tags') +
         api.properties(
             patch_text='hihihi\nfoo:bar\nbam:baz',
             footer='foo'
         ) +
         api.step_data(
             'parse description',
             api.json.output({'Foo': ['bar']})) +
         api.step_data(
             'parse description (2)',
             api.json.output({'Foo': ['bar']}))
  )
