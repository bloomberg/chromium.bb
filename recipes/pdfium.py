# Copyright 2015 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import sys

import recipe_util  # pylint: disable=F0401


# This class doesn't need an __init__ method, so we disable the warning
# pylint: disable=W0232
class PDFium(recipe_util.Recipe):
  """Basic Recipe class for PDFium."""

  @staticmethod
  def fetch_spec(props):
    url = 'https://pdfium.googlesource.com/pdfium.git'
    solution = {
        'name'        : 'pdfium',
        'url'         : url,
        'deps_file'   : 'DEPS',
        'managed'     : False,
        'custom_deps' : {},
        'safesync_url': '',
    }
    spec = {
      'solutions': [solution],
    }
    return {
      'type': 'gclient_git',
      'gclient_git_spec': spec,
    }

  @staticmethod
  def expected_root(_props):
    return 'pdfium'


def main(argv=None):
  return PDFium().handle_args(argv)


if __name__ == '__main__':
  sys.exit(main(sys.argv))
