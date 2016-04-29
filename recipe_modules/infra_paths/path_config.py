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
  # TODO(phajdan.jr): set persistent cache path for remaining platforms.
  # NOTE: do not use /b/swarm_slave here - it gets deleted on bot redeploy,
  # and may happen even after a reboot.
  if c.PLATFORM == 'linux':
    c.base_paths['cache'] = (
        '/', 'b', 'cache', 'chromium')
    c.base_paths['git_cache'] = (
        '/', 'b', 'cache', 'chromium', 'git_cache')
    c.base_paths['goma_cache'] = (
        '/', 'b', 'cache', 'chromium', 'goma_cache')
    c.base_paths['goma_deps_cache'] = (
        '/', 'b', 'cache', 'chromium', 'goma_deps_cache')
  else:
    c.base_paths['cache'] = c.base_paths['root'] + ('cache',)
    c.base_paths['git_cache'] = c.base_paths['root'] + ('cache_dir',)
    c.base_paths['goma_cache'] = c.base_paths['root'] + ('goma_cache',)
    c.base_paths['goma_deps_cache'] = c.base_paths['root'] + (
        'goma_deps_cache',)
