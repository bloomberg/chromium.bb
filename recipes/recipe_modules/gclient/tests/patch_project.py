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
  'patch_project': recipe_api.Property(None),
  'patch_repository_url': recipe_api.Property(None),
}


def RunSteps(api, patch_project, patch_repository_url):
  src_cfg = api.gclient.make_config(CACHE_DIR='[ROOT]/git_cache')
  soln = src_cfg.solutions.add()
  soln.name = 'src'
  soln.url = 'https://chromium.googlesource.com/chromium/src.git'
  src_cfg.patch_projects['v8'] = ('src/v8', 'HEAD')
  src_cfg.patch_projects['v8/v8'] = ('src/v8', 'HEAD')
  src_cfg.repo_path_map.update({
      'https://chromium.googlesource.com/v8/v8': ('src/v8', 'HEAD'),
      # non-canonical URL
      'https://webrtc.googlesource.com/src.git': (
          'src/third_party/webrtc', 'HEAD'),
  })
  assert api.gclient.get_repo_path(
      'https://chromium.googlesource.com/chromium/src.git',
      gclient_config=src_cfg) == 'src'
  assert api.gclient.get_repo_path(
      'https://chromium.googlesource.com/chromium/src',
      gclient_config=src_cfg) == 'src'
  assert api.gclient.get_repo_path(
      'https://chromium.googlesource.com/v8/v8',
      gclient_config=src_cfg) == 'src/v8'
  assert api.gclient.get_repo_path(
      'https://webrtc.googlesource.com/src',
      gclient_config=src_cfg) == 'src/third_party/webrtc'
  assert api.gclient.get_repo_path(
      'https://example.googlesource.com/unrecognized',
      gclient_config=src_cfg) is None

  api.gclient.c = src_cfg

  api.gclient.calculate_patch_root(
      patch_project, None, patch_repository_url)

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

  yield (
      api.test('webrtc') +
      api.properties(
          patch_repository_url='https://webrtc.googlesource.com/src') +
      api.post_process(post_process.DropExpectation)
  )
