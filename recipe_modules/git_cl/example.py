# Copyright 2016 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from recipe_engine.config_types import Path


DEPS = [
  'git_cl',
  'recipe_engine/path',
  'recipe_engine/raw_io',
  'recipe_engine/step',
]


def RunSteps(api):
  res = api.git_cl.get_description(cwd=api.path.mkdtemp('fakee'))
  # Look ma, no hands! (Can pass in the cwd without configuring git_cl).
  api.step('echo', ['echo', res.stdout])

  api.git_cl.set_config('basic')
  api.git_cl.c.repo_location = api.path.mkdtemp('fakerepo')

  api.step('echo', ['echo', api.git_cl.get_description().stdout])

  api.git_cl.set_description("new description woo")

  api.step('echo', ['echo', api.git_cl.get_description().stdout])

def GenTests(api):
  yield (
      api.test('basic') +
      api.override_step_data(
          'git_cl description', stdout=api.raw_io.output('hi')) +
      api.override_step_data(
          'git_cl description (2)', stdout=api.raw_io.output('hey')) +
      api.override_step_data(
          'git_cl description (3)', stdout=api.raw_io.output(
              'new description woo'))
  )

