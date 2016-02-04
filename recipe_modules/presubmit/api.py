# Copyright 2016 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from recipe_engine import recipe_api

class PresubmitApi(recipe_api.RecipeApi):
  def __call__(self, *args, **kwargs):
    """Return a presubmit step."""
    name = kwargs.pop('name', 'presubmit')
    return self.m.python(
        name, self.package_resource('presubmit_support.py'), list(args),
        **kwargs)
