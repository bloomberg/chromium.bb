# Copyright 2016 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from recipe_engine import recipe_api


class InfraPathsApi(recipe_api.RecipeApi):
  """infra_paths module is glue for design mistakes. It will be removed."""

  def initialize(self):
    # TODO(phajdan.jr): remove dupes from the engine and delete infra_ prefix.
    self.m.path.set_config(
        'infra_' + self.m.properties.get('path_config', 'buildbot'))

  @property
  def default_git_cache_dir(self):
    """Returns the location of the default git cache directory.

    This property should be used instead of using path['git_cache'] directly.

    It returns git_cache path if it is defined (Buildbot world), otherwise
    uses the more generic [CACHE]/git path (LUCI world).
    """
    try:
      return self.m.path['git_cache']
    except KeyError:  # pragma: no cover | TODO(nodir): cover
      return self.m.path['cache'].join('git')
