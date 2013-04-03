# Copyright (c) 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# pylint: disable=F0401
import recipe_util
import sys

# pylint: disable=W0232
class Chromium(recipe_util.Recipe):
  """Basic Recipe class for Chromium."""

  @staticmethod
  def fetch_spec(props):
    url = 'https://chromium.googlesource.com/chromium/src.git'
    solution = { 'name'   :'src',
                 'url'    : url,
                 'deps_file': '.DEPS.git',
                 'managed'   : True,
                 'custom_deps': {},
                 'safesync_url': '',
    }
    if props.get('webkit_rev', '') == 'ToT':
      solution['custom_vars'] = {'webkit_rev': ''}
    spec = {
      'solutions': [solution],
      'svn_url': 'svn://svn.chromium.org/chrome',
      'svn_branch': 'trunk/src',
      'svn_ref': 'git-svn',
    }
    if props.get('submodule_git_svn_spec'):
      spec['submodule_git_svn_spec'] = props['submodule_git_svn_spec']
    return {
      'type': 'gclient_git_svn',
      'gclient_git_svn_spec': spec
    }

  @staticmethod
  def expected_root(_props):
    return 'src'


def main(argv=None):
  return Chromium().handle_args(argv)


if __name__ == '__main__':
  sys.exit(main(sys.argv))
