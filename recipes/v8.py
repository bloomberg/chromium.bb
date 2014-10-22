# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import sys

import recipe_util  # pylint: disable=F0401


# This class doesn't need an __init__ method, so we disable the warning
# pylint: disable=W0232
class V8(recipe_util.Recipe):
  """Basic Recipe class for V8."""

  @staticmethod
  def fetch_spec(props):
    url = 'https://chromium.googlesource.com/v8/v8.git'
    solution = {
        'name'        : 'v8',
        'url'         : url,
        'deps_file'   : '.DEPS.git',
        'managed'     : False,
        'custom_deps' : {},
        'safesync_url': '',
    }
    spec = {
      'solutions': [solution],
      'with_branch_heads': True,
      'svn_url': 'https://v8.googlecode.com/svn',
      'git_svn_fetch': {
        'branches/bleeding_edge': 'refs/remotes/origin/master',
        'trunk': 'refs/remotes/origin/candidates',
        'branches/3.28': 'refs/remotes/branch-heads/3.28',
        'branches/3.29': 'refs/remotes/branch-heads/3.29',
      },
      'git_svn_branches': {},
    }
    checkout_type = 'gclient_git_svn'
    if props.get('nosvn'):
      checkout_type = 'gclient_git'
    spec_type = '%s_spec' % checkout_type
    return {
      'type': checkout_type,
      spec_type: spec,
    }

  @staticmethod
  def expected_root(_props):
    return 'v8'


def main(argv=None):
  return V8().handle_args(argv)


if __name__ == '__main__':
  sys.exit(main(sys.argv))
