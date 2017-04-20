# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

DEPS = [
  'recipe_engine/step',
  'recipe_engine/path',
  'url',
]

def RunSteps(api):
  api.step('step1',
           ['/bin/echo', api.url.join('foo', 'bar', 'baz')])
  api.step('step2',
           ['/bin/echo', api.url.join('foo/', '/bar/', '/baz')])
  api.step('step3',
           ['/bin/echo', api.url.join('//foo/', '//bar//', '//baz//')])
  api.step('step4',
           ['/bin/echo', api.url.join('//foo/bar//', '//baz//')])
  api.url.fetch('fake://foo/bar', attempts=5)
  api.url.fetch('fake://foo/bar (w/ auth)', headers={'Authorization': 'thing'})
  api.url.fetch_to_file('fake://foo/bar', api.path['start_dir'],
      headers={'Authorization': 'thing'}, attempts=10)

def GenTests(api):
  yield api.test('basic')
