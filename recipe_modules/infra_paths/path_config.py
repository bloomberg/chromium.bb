# Copyright 2016 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import DEPS
CONFIG_CTX = DEPS['path'].CONFIG_CTX


@CONFIG_CTX()
def infra_common(c):
  c.dynamic_paths['checkout'] = None


@CONFIG_CTX(includes=['infra_common'])
def infra_buildbot(c):
  c.base_paths['root'] = c.CURRENT_WORKING_DIR[:-4]
  c.base_paths['slave_build'] = c.CURRENT_WORKING_DIR
  c.base_paths['cache'] = c.base_paths['root'] + (
      'build', 'slave', 'cache')
  c.base_paths['git_cache'] = c.base_paths['root'] + (
      'build', 'slave', 'cache_dir')
  c.base_paths['goma_cache'] = c.base_paths['root'] + (
      'build', 'slave', 'goma_cache')
  for token in ('build_internal', 'build', 'depot_tools'):
    c.base_paths[token] = c.base_paths['root'] + (token,)


@CONFIG_CTX(includes=['infra_common'])
def infra_kitchen(c):
  c.base_paths['root'] = c.CURRENT_WORKING_DIR
  c.base_paths['slave_build'] = c.CURRENT_WORKING_DIR
  # TODO(phajdan.jr): have one cache dir, let clients append suffixes.

  b_dir = c.CURRENT_WORKING_DIR
  while b_dir and b_dir[-1] != 'b':
    b_dir = b_dir[:-1]

  if c.PLATFORM in ('linux', 'mac'):
    c.base_paths['cache'] = ('/', 'b', 'c')
    c.base_paths['builder_cache'] = c.base_paths['cache'] + ('b',)
    for path in ('git_cache', 'goma_cache', 'goma_deps_cache'):
      c.base_paths[path] = c.base_paths['cache'] + (path,)
  elif b_dir:
    c.base_paths['cache'] = b_dir + ('c',)
    c.base_paths['builder_cache'] = c.base_paths['cache'] + ('b',)
    for path in ('git_cache', 'goma_cache', 'goma_deps_cache'):
      c.base_paths[path] = c.base_paths['cache'] + (path,)
  else:  # pragma: no cover
    c.base_paths['cache'] = c.base_paths['root'] + ('c',)
    c.base_paths['builder_cache'] = c.base_paths['cache'] + ('b',)
    c.base_paths['git_cache'] = c.base_paths['root'] + ('cache_dir',)
    for path in ('goma_cache', 'goma_deps_cache'):
      c.base_paths[path] = c.base_paths['cache'] + (path,)


@CONFIG_CTX(includes=['infra_buildbot'])
def infra_swarmbucket(c):
  c.base_paths['git_cache'] = c.base_paths['root'] + ('git_cache',)
