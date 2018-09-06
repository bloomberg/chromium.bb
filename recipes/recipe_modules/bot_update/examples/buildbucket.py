# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

DEPS = [
  'bot_update',
  'gclient',
  'recipe_engine/buildbucket',
]

def RunSteps(api):
  cfg = api.gclient.make_config(CACHE_DIR='[GIT_CACHE]')
  soln = cfg.solutions.add()
  soln.name = 'v8'
  soln.url = 'https://chromium.googlesource.com/v8/v8'
  api.gclient.c = cfg
  api.bot_update.ensure_checkout()


def GenTests(api):
  yield api.test('ci') + api.buildbucket.ci_build(
      'v8', 'ci', 'builder',
      git_repo='https://chromium.googlesource.com/v8/v8',
      revision='2d72510e447ab60a9728aeea2362d8be2cbd7789')

  yield api.test('ci with invalid repo') + api.buildbucket.ci_build(
      'v8', 'ci', 'builder',
      git_repo='https://chromium.googlesource.com/trash',
      revision='2d72510e447ab60a9728aeea2362d8be2cbd7789')

  yield api.test('ci without repo') + api.buildbucket.build(
      api.buildbucket.build_pb2.Build(
          builder=api.buildbucket.build_pb2.BuilderID(
              project='v8',
              bucket='ci',
              builder='builder',
          ),
          input=api.buildbucket.build_pb2.Build.Input(
            gitiles_commit=api.buildbucket.common_pb2.GitilesCommit(
                id='2d72510e447ab60a9728aeea2362d8be2cbd7789',
            ),
          ),
      ))

  yield api.test('try') + api.buildbucket.try_build(
      'v8', 'try', 'builder',
      gerrit_host='chromium-review.googlesource.com')
