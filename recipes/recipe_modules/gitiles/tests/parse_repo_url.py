# Copyright 2018 The LUCI Authors. All rights reserved.
# Use of this source code is governed under the Apache License, Version 2.0
# that can be found in the LICENSE file.

DEPS = [
  'gitiles',
  'recipe_engine/properties',
  'recipe_engine/step',
]


def RunSteps(api):
  repo_url = api.properties['repo_url']
  host, project = api.gitiles.parse_repo_url(repo_url)
  api.step('build', ['echo', str(host), str(project)])


def GenTests(api):

  def case(name, repo_url):
    return api.test(name) + api.properties(repo_url=repo_url)

  yield case('basic', 'https://host/path/to/project')
  yield case('http', 'http://host/path/to/project')
  yield case('a prefix', 'https://host/a/path/to/project')
  yield case('git suffix', 'https://host/path/to/project.git')
  yield case('http and a prefix', 'http://host/a/path/to/project')
  yield case('no scheme', 'host/a/path/to/project')
  yield case('query string param', 'https://host/a/path/to/project?a=b')
  yield case('plus', 'https://host/path/to/project/+/master')
