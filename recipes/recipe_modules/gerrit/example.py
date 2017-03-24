# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

DEPS = [
    'gerrit'
]


def RunSteps(api):
  host = 'https://chromium-review.googlesource.com/a'
  project = 'v8/v8'

  branch = 'test'
  commit = '67ebf73496383c6777035e374d2d664009e2aa5c'

  data = api.gerrit.create_gerrit_branch(host, project, branch, commit)
  assert data == 'refs/heads/test'

  data = api.gerrit.get_gerrit_branch(host, project, 'master')
  assert data == '67ebf73496383c6777035e374d2d664009e2aa5c'


def GenTests(api):
  yield (
      api.test('basic')
      + api.step_data(
          'gerrit create_gerrit_branch',
          api.gerrit.make_gerrit_create_branch_response_data()
      )
      + api.step_data(
          'gerrit get_gerrit_branch',
          api.gerrit.make_gerrit_get_branch_response_data()
      )
  )
