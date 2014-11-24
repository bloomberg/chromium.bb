# Copyright (c) 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import sys

import recipe_util  # pylint: disable=F0401


# This class doesn't need an __init__ method, so we disable the warning
# pylint: disable=W0232
class WebRTC(recipe_util.Recipe):
  """Basic Recipe class for WebRTC."""

  @staticmethod
  def fetch_spec(props):
    url = 'https://chromium.googlesource.com/external/webrtc.git'
    spec = {
      'solutions': [
        {
          'name': 'src',
          'url': url,
          'deps_file': 'DEPS',
          'managed': False,
          'custom_deps': {},
          'safesync_url': '',
        },
      ],
      'auto': True,  # Runs git auto-svn as a part of the fetch.
    }

    if props.get('target_os'):
      spec['target_os'] = props['target_os'].split(',')

    return {
      'type': 'gclient_git_svn',
      'gclient_git_svn_spec': spec,
    }

  @staticmethod
  def expected_root(_props):
    return 'src'


def main(argv=None):
  return WebRTC().handle_args(argv)


if __name__ == '__main__':
  sys.exit(main(sys.argv))
