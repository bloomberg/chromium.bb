# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

DEPS = [
    'gerrit',
    'recipe_engine/step',
]


def RunSteps(api):
  host = 'https://chromium-review.googlesource.com'
  project = 'v8/v8'

  branch = 'test'
  commit = '67ebf73496383c6777035e374d2d664009e2aa5c'

  data = api.gerrit.create_gerrit_branch(host, project, branch, commit)
  assert data == 'refs/heads/test'

  data = api.gerrit.get_gerrit_branch(host, project, 'master')
  assert data == '67ebf73496383c6777035e374d2d664009e2aa5c'

  # Query for changes in Chromium's CQ.
  api.gerrit.get_changes(
      host,
      query_params=[
        ('project', 'chromium/src'),
        ('status', 'open'),
        ('label', 'Commit-Queue>0'),
      ],
      start=1,
      limit=1,
  )

  # Query which returns no changes is still successful query.
  empty_list = api.gerrit.get_changes(
      host,
      query_params=[
        ('project', 'chromium/src'),
        ('status', 'open'),
        ('label', 'Commit-Queue>2'),
      ],
      name='changes empty query',
  )
  assert len(empty_list) == 0

  api.gerrit.get_change_description(
      host, change=123, patchset=1)

  api.gerrit.abandon_change(host, 123, 'bad roll')

  with api.step.defer_results():
    api.gerrit.get_change_description(
        host, change=122, patchset=3)


def GenTests(api):
  yield (
      api.test('basic')
      + api.step_data(
          'gerrit create_gerrit_branch (v8/v8 test)',
          api.gerrit.make_gerrit_create_branch_response_data()
      )
      + api.step_data(
          'gerrit get_gerrit_branch (v8/v8 master)',
          api.gerrit.make_gerrit_get_branch_response_data()
      )
      + api.step_data(
        'gerrit changes empty query',
        api.gerrit.get_empty_changes_response_data()
      )
  )
