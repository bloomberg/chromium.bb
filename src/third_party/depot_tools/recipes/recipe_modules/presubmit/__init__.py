# Copyright 2019 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from recipe_engine.config import ConfigGroup, Single
from recipe_engine.recipe_api import Property

from PB.recipe_modules.depot_tools.presubmit import properties


DEPS = [
  'bot_update',
  'depot_tools',
  'gclient',
  'git',
  'recipe_engine/context',
  'recipe_engine/cq',
  'recipe_engine/json',
  'recipe_engine/path',
  'recipe_engine/properties',
  'recipe_engine/python',
  'recipe_engine/step',
  'tryserver',
]


PROPERTIES = properties.InputProperties
