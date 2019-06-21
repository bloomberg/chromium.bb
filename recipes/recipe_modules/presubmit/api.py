# Copyright 2016 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from recipe_engine import recipe_api

class PresubmitApi(recipe_api.RecipeApi):
  @property
  def presubmit_support_path(self):
    return self.repo_resource('presubmit_support.py')

  def __call__(self, *args, **kwargs):
    """Return a presubmit step."""

    name = kwargs.pop('name', 'presubmit')
    with self.m.depot_tools.on_path():
      presubmit_args = list(args) + [
          '--json_output', self.m.json.output(),
      ]
      step_data = self.m.python(
          name, self.presubmit_support_path, presubmit_args, **kwargs)
      return step_data.json.output
