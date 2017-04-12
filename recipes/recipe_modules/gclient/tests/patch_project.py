# Copyright 2017 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from recipe_engine import post_process
from recipe_engine import recipe_api


DEPS = [
  'gclient',
  'recipe_engine/properties',
]


PROPERTIES = {
  'patch_project': recipe_api.Property(),
}


def RunSteps(api, patch_project):
  api.gclient.set_config('chromium')

  patch_root = api.gclient.calculate_patch_root(patch_project)

  api.gclient.set_patch_project_revision(patch_project)


def GenTests(api):
  yield (
      api.test('chromium') +
      api.properties(patch_project='chromium') +
      api.post_process(post_process.DropExpectation)
  )

  yield (
      api.test('v8') +
      api.properties(patch_project='v8') +
      api.post_process(post_process.DropExpectation)
  )
