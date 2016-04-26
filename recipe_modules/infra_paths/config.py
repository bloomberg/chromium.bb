# Copyright 2016 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from recipe_engine.config import config_item_context, ConfigGroup, Dict, Static
from recipe_engine.config_types import Path

def BaseConfig(PLATFORM, CURRENT_WORKING_DIR, ROOT, **_kwargs):
  return ConfigGroup(
    paths    = Dict(value_type=Path),

    PLATFORM = Static(PLATFORM),
    CURRENT_WORKING_DIR = Static(CURRENT_WORKING_DIR),
    ROOT = Static(ROOT),
  )

config_ctx = config_item_context(BaseConfig)

@config_ctx()
def buildbot(c):
  c.paths['root'] = c.ROOT.join('b')
  c.paths['slave_build'] = c.CURRENT_WORKING_DIR
  c.paths['cache'] = c.paths['root'].join(
      'build', 'slave', 'cache')
  c.paths['git_cache'] = c.paths['root'].join(
      'build', 'slave', 'cache_dir')
  c.paths['goma_cache'] = c.paths['root'].join(
      'build', 'slave', 'goma_cache')
  for token in ('build_internal', 'build', 'depot_tools'):
    c.paths[token] = c.paths['root'].join(token,)

@config_ctx()
def kitchen(c):
  c.paths['root'] = c.CURRENT_WORKING_DIR
  c.paths['slave_build'] = c.CURRENT_WORKING_DIR
  # TODO(phajdan.jr): have one cache dir, let clients append suffixes.
  # TODO(phajdan.jr): set persistent cache path for remaining platforms.
  # NOTE: do not use /b/swarm_slave here - it gets deleted on bot redeploy,
  # and may happen even after a reboot.
  if c.PLATFORM == 'linux':
    c.paths['cache'] = c.ROOT.join(
        'b', 'cache', 'chromium')
    c.paths['git_cache'] = c.ROOT.join(
        'b', 'cache', 'chromium', 'git_cache')
    c.paths['goma_cache'] = c.ROOT.join(
        'b', 'cache', 'chromium', 'goma_cache')
  else:
    c.paths['cache'] = c.paths['root'].join('cache')
    c.paths['git_cache'] = c.paths['root'].join('cache_dir')
    c.paths['goma_cache'] = c.paths['root'].join('goma_cache')
