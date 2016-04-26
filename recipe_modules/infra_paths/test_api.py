# Copyright 2016 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from recipe_engine import recipe_test_api
from recipe_engine.config_types import Path, NamedBasePath

class InfraPathsTestApi(recipe_test_api.RecipeTestApi): #pragma: no cover
  @recipe_test_api.mod_test_data
  @staticmethod
  def exists(*paths):
    assert all(isinstance(p, Path) for p in paths)
    return paths

  def __getitem__(self, name):
    return Path(NamedBasePath(name))
