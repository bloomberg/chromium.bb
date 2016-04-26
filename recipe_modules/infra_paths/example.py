# Copyright 2016 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

DEPS = [
  'infra_paths',
  'recipe_engine/platform',
  'recipe_engine/properties',
  'recipe_engine/step',
]

from recipe_engine.config_types import Path

def RunSteps(api):
  api.step('step', [], cwd=api.infra_paths['slave_build'])


def GenTests(api):
  for platform in ('linux', 'win', 'mac'):
    yield (api.test(platform) + api.platform.name(platform))

    yield (api.test('%s_kitchen' % platform) +
           api.platform.name(platform) +
           api.properties(path_config='kitchen'))
