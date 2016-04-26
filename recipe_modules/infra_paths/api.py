# Copyright 2016 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from recipe_engine import recipe_api
from recipe_engine.config_types import Path, NamedBasePath


class InfraPathsApi(recipe_api.RecipeApi):
  def get_config_defaults(self):
    return {
      'PLATFORM': self.m.platform.name,
      'CURRENT_WORKING_DIR': self.m.path['cwd'],
      'ROOT': self.m.path['root'],
    }

  def __init__(self, **kwargs):
    super(InfraPathsApi, self).__init__(**kwargs)
    self._config_set = False

  def _lazy_set_config(self):
    if self._config_set:
      return
    self._config_set = True

    path_config = self.m.properties.get('path_config')
    if path_config in ('kitchen',):
      self.set_config(path_config)
    else:
      self.set_config('buildbot')

    if self._test_data.enabled:
      for path in self._test_data.get('exists', []):
        assert isinstance(path.base, NamedBasePath)
        self.m.path.mock_add_paths(self[path.base.name].join(*path.pieces))

  def __getitem__(self, name):
    self._lazy_set_config()
    return self.c.paths[name]
