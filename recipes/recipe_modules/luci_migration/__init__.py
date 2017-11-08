# Copyright 2017 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

DEPS = [
  'recipe_engine/path',
  'recipe_engine/properties',
]

from recipe_engine.recipe_api import Property
from recipe_engine.config import ConfigGroup, Single

PROPERTIES = {
  '$depot_tools/luci_migration': Property(
    help='Properties specifically for the luci_migration module',
    param_name='migration_properties',
    kind=ConfigGroup(
      # Whether builder runs on LUCI stack.
      is_luci=Single(bool),
      # Whether builder is in production.
      is_prod=Single(bool),
    ),
    default={},
  ),
}
