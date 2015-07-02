# Copyright (c) 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import json
import sys

import recipe_util  # pylint: disable=F0401


# This class doesn't need an __init__ method, so we disable the warning
# pylint: disable=W0232
class Blink(recipe_util.Recipe):
  """Basic Recipe alias for Blink -> Chromium."""

  @staticmethod
  def fetch_spec(props):
    chromium_url = 'https://chromium.googlesource.com/chromium/src.git'
    chromium_solution = {'name': 'src',
                         'url': chromium_url,
                         'deps_file': 'DEPS',
                         'managed': False,
                         'custom_deps': {
                             'src/third_party/WebKit': None,
                         },
    }
    blink_url = 'https://chromium.googlesource.com/chromium/blink.git'
    blink_solution = {'name': 'src/third_party/WebKit',
                      'url': blink_url,
                      'deps_file': '.DEPS.git',
                      'managed': False,
                      'custom_deps': {},
    }
    spec = {
      'solutions': [chromium_solution, blink_solution],
      'auto': True,
    }
    if props.get('target_os'):
      spec['target_os'] = props['target_os'].split(',')
    if props.get('target_os_only'):
      spec['target_os_only'] = props['target_os_only']
    toolchain_hook = [sys.executable, 'src/build/confirm_toolchain.py']
    spec['fetch_hooks'] = [toolchain_hook]
    return {
      'type': 'gclient_git_svn',
      'gclient_git_svn_spec': spec,
    }

  @staticmethod
  def expected_root(_props):
    return 'src/third_party/WebKit'


def main(argv=None):
  return Blink().handle_args(argv)


if __name__ == '__main__':
  sys.exit(main(sys.argv))
