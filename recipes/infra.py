# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import sys

import recipe_util  # pylint: disable=F0401


# This class doesn't need an __init__ method, so we disable the warning
# pylint: disable=W0232
class Infra(recipe_util.Recipe):
  """Basic Recipe class for the Infrastructure repositories."""

  @staticmethod
  def fetch_spec(_props):
    solution = lambda name, path_infix = None: {
      'name'     : name,
      'url'      : 'https://chromium.googlesource.com/infra/%s%s.git' % (
        path_infix + '/' if path_infix else '', name
      ),
      'deps_file': '.DEPS.git',
      'managed'  : False,
    }
    spec = {
        'solutions': [
          solution('infra'),
          solution('expect_tests', 'testing'),
          solution('testing_support', 'testing'),
        ],
    }
    return {
        'type': 'gclient_git',
        'gclient_git_spec': spec,
    }

  @staticmethod
  def expected_root(_props):
    return 'infra'


def main(argv=None):
  return Infra().handle_args(argv)


if __name__ == '__main__':
  sys.exit(main(sys.argv))
