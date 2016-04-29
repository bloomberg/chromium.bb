# Copyright 2016 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from recipe_engine import recipe_api


class InfraPathsApi(recipe_api.RecipeApi):
  def initialize(self):
    # TODO(phajdan.jr): remove dupes from the engine and delete infra_ prefix.
    self.m.path.set_config(
        'infra_' + self.m.properties.get('path_config', 'buildbot'))
