# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

DEPS = [
  'infra_paths',
  'recipe_engine/path',
  'recipe_engine/properties',
  'recipe_engine/step',
  'rietveld',
]

def RunSteps(api):
  api.path['checkout'] = api.path['start_dir']
  api.rietveld.apply_issue(
      'foo', 'bar', authentication=api.properties.get('authentication'))
  api.rietveld.calculate_issue_root({'project': ['']})


def GenTests(api):
  yield api.test('basic') + api.properties(
      issue=1,
      patchset=1,
      rietveld='http://review_tool.url',
      authentication='oauth2',
  )
  yield api.test('no_auth') + api.properties(
      issue=1,
      patchset=1,
      rietveld='http://review_tool.url',
  )
  yield api.test('buildbot') + api.properties(
      path_config='buildbot',
      issue=1,
      patchset=1,
      rietveld='http://review_tool.url',
      authentication='oauth2',
  )
