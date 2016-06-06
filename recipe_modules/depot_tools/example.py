# Copyright 2016 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

DEPS = [
  'depot_tools',
  'recipe_engine/step',
  'recipe_engine/path',
  'recipe_engine/platform',
]


def RunSteps(api):
  api.step(
      'download_from_google_storage',
      ['ls', api.depot_tools.download_from_google_storage_path])

  api.step(
      'upload_to_google_storage',
      ['ls', api.depot_tools.upload_to_google_storage_path])

  api.step('cros', ['ls', api.depot_tools.cros_path])

  api.step(
      'gn_py_path', ['ls', api.depot_tools.gn_py_path])

  api.step(
      'gsutil_py_path', ['ls', api.depot_tools.gsutil_py_path])

  api.step(
      'ninja_path', ['ls', api.depot_tools.ninja_path])



def GenTests(api):
  yield api.test('basic')

  yield api.test('win') + api.platform('win', 32)
