# Copyright 2016 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

DEPS = [
  'infra_paths',
  'recipe_engine/path',
  'recipe_engine/platform',
  'recipe_engine/properties',
  'recipe_engine/step',
]


def RunSteps(api):
  api.step('show cache path', [])
  api.step.active_result.presentation.step_text = str(api.path['cache'])


def GenTests(api):
  yield api.test('basic')

  for platform in ('linux', 'mac', 'win'):
    for path_config in ('buildbot', 'kitchen', 'swarmbucket'):
      yield (
          api.test('paths_%s_%s' % (path_config, platform)) +
          api.platform.name(platform) +
          api.properties(path_config=path_config)
      )
