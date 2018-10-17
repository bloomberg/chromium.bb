# Copyright 2018 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from recipe_engine import post_process

DEPS = [
  'bot_update',
  'gclient',
  'recipe_engine/json',
]


def RunSteps(api):
  api.gclient.set_config('depot_tools')
  api.bot_update.ensure_checkout()


def GenTests(api):
  yield (
      api.test('basic') +
      api.post_process(post_process.StatusCodeIn, 0) +
      api.post_process(post_process.DropExpectation)
  )

  yield (
      api.test('failure') +
      api.override_step_data(
          'bot_update',
          api.json.output({'did_run': True}),
          retcode=1) +
      api.post_process(post_process.StatusCodeIn, 1) +
      api.post_process(post_process.DropExpectation)
  )
