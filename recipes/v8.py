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
    ref = 'branch-heads/bleeding_edge'
    url = 'https://chromium.googlesource.com/external/v8.git@%s' % ref
    solution = { 'name'   :'v8',
                 'url'    : url,
                 'deps_file': '.DEPS.git',
                 'managed'   : False,
                 'custom_deps': {},
                 'safesync_url': '',
    }
    spec = {
      'solutions': [solution],
      'svn_url': 'https://v8.googlecode.com/svn',
      'svn_branch': 'branches/bleeding_edge',
      'svn_ref': 'bleeding_edge',
      'svn_prefix': 'branch-heads/',
      'with_branch_heads': True,
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
