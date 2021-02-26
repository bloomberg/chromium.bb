# Copyright 2020 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from recipe_engine import post_process

DEPS = [
  'recipe_engine/assertions',
  'recipe_engine/buildbucket',
  'recipe_engine/path',
  'recipe_engine/platform',
  'recipe_engine/properties',
  'recipe_engine/raw_io',

  'gclient',
]

def RunSteps(api):
  src_cfg = api.gclient.make_config(CACHE_DIR=api.path['cache'].join('git'))

  soln = src_cfg.solutions.add()
  soln.name = 'src'
  soln.url = 'https://chromium.googlesource.com/chromium/src.git'
  src_cfg.repo_path_map.update({
      'https://chromium.googlesource.com/src': ('src', 'HEAD'),
      'https://chromium.googlesource.com/v8/v8': ('src/v8', 'HEAD'),
      # non-canonical URL
      'https://webrtc.googlesource.com/src.git': (
          'src/third_party/webrtc', 'HEAD'),
  })

  api.gclient.c = src_cfg
  affected_files = api.gclient.diff_deps(api.path['cache'])

  api.assertions.assertEqual(
      affected_files,
      list(api.properties.get('diff_deps_files')),
  )

def GenTests(api):
  test_files = (
      'third_party/mockfile1',
      'third_party/mockfile2'
  )
  no_test_files = []

  yield api.test(
      'basic',
      api.buildbucket.try_build(),
      api.properties(diff_deps_files=test_files),
      api.override_step_data(
          'gclient recursively git diff all DEPS',
          api.gclient.diff_deps_test_data(test_files),
      ),
      api.post_process(post_process.StatusSuccess),
  )

  yield api.test(
      'no change, exception',
      api.buildbucket.try_build(),
      api.properties(diff_deps_files=no_test_files),
      api.override_step_data(
          'gclient recursively git diff all DEPS',
          api.gclient.diff_deps_test_data(no_test_files),
      ),
      api.expect_exception('DepsDiffException')
  )
  yield api.test(
      'dont have revision yet',
      api.buildbucket.try_build(),
      api.properties(diff_deps_files=test_files),
      api.override_step_data(
          'gclient recursively git diff all DEPS',
          api.raw_io.stream_output('fatal: bad object abcdef1234567890'),
      ),
      api.expect_exception('DepsDiffException')
  )
  yield api.test(
      'windows',
      api.buildbucket.try_build(),
      api.properties(diff_deps_files=test_files),
      api.platform.name('win'),
      api.override_step_data(
          'gclient recursively git diff all DEPS',
          api.gclient.diff_deps_test_data(test_files),
      ),
      api.post_process(post_process.StatusSuccess),
  )
