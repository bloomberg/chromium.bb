# Copyright 2015 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import sys

import recipe_util  # pylint: disable=F0401


# This class doesn't need an __init__ method, so we disable the warning
# pylint: disable=W0232
class PdfiumRecipe(recipe_util.Recipe):
  """Basic Recipe class for pdfium."""

  @staticmethod
  def fetch_spec(props):
    return {
      'type': 'gclient_git',
      'gclient_git_spec': {
        'solutions': [
          {
            'name': 'pdfium',
            'url': 'https://pdfium.googlesource.com/pdfium.git',
            'managed': False,
          },
        ],
      },
    }

  @staticmethod
  def expected_root(_props):
    return 'pdfium'


def main(argv=None):
  return PdfiumRecipe().handle_args(argv)


if __name__ == '__main__':
  sys.exit(main(sys.argv))
