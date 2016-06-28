# Copyright 2016 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from recipe_engine import recipe_api

import string

class GitClApi(recipe_api.RecipeApi):
  def __call__(self, subcmd, args, name=None, **kwargs):
    if not name:
      name = 'git_cl ' + subcmd

    if kwargs.get('suffix'):
      name = name + ' (%s)' % kwargs.pop('suffix')

    if 'cwd' not in kwargs:
      kwargs['cwd'] = (self.c and self.c.repo_location) or None

    return self.m.step(
        name, [self.package_repo_resource('git_cl.py'), subcmd] + args,
        **kwargs)

  def get_description(self, patch=None, codereview=None, **kwargs):
    args = ['-d']
    if patch or codereview:
      assert patch and codereview, "Both patch and codereview must be provided"
      args.append('--%s' % codereview)
      args.append(patch)

    return self('description', args, stdout=self.m.raw_io.output(), **kwargs)

  def set_description(self, description, patch=None, codereview=None, **kwargs):
    args = ['-n', '-']
    if patch or codereview:
      assert patch and codereview, "Both patch and codereview must be provided"
      args.append(patch)
      args.append('--%s' % codereview)

    return self(
        'description', args, stdout=self.m.raw_io.output(),
        stdin=self.m.raw_io.input(data=description),
        name='git_cl set description', **kwargs)

  def upload(self, message, upload_args=None, **kwargs):
    upload_args = upload_args or []

    upload_args.extend(['--message-file', self.m.raw_io.input(message)])

    return self('upload', upload_args, **kwargs)

  def issue(self, **kwargs):
    return self('issue', [], stdout=self.m.raw_io.output(), **kwargs)

