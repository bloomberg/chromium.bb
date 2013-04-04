# Copyright (c) 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import json
# pylint: disable=F0401
import recipe_util
import sys

# pylint: disable=W0232
class Blink(recipe_util.Recipe):
  """Basic Recipe alias for Blink -> Chromium."""

  @staticmethod
  def fetch_spec(props):
    submodule_spec = {
      'third_party/WebKit': {
        'svn_url': 'svn://svn.chromium.org/blink/trunk',
        'svn_branch': 'trunk',
        'svn_ref': 'master',
      }
    }
    return {'alias': {
        'recipe': 'chromium',
        'props': ['--webkit_rev=ToT',
                  '--submodule_git_svn_spec=' + json.dumps(submodule_spec)]
      }
    }


def main(argv=None):
  Blink().handle_args(argv)


if __name__ == '__main__':
  sys.exit(main(sys.argv))
