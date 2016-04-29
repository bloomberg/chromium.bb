# Copyright 2016 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from recipe_engine import recipe_api

class GitClApi(recipe_api.RecipeApi):
  def __call__(self, subcmd, args, name=None, **kwargs):
    if not name:
      name = 'git_cl ' + subcmd
    if 'cwd' not in kwargs:
      kwargs['cwd'] = (self.c and self.c.repo_location) or None

    return self.m.step(
        name, [self.package_repo_resource('git_cl.py')] + args, **kwargs)

  def get_description(self, **kwargs):
    return self('description', ['-d'], stdout=self.m.raw_io.output(), **kwargs)

  def set_description(self, description, **kwargs):
    return self(
        'description', ['-n', '-'],
        stdout=self.m.raw_io.output(),
        stdin=self.m.raw_io.input(data=description),
        name='git_cl set description', **kwargs)
