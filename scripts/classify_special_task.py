# Copyright 2017 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Helper script to classify the results of  a special task."""

from __future__ import print_function

import os

from chromite.lib import classifier
from chromite.lib import commandline
from chromite.lib import gs

def GetParser():
  """Creates the argparse parser."""
  parser = commandline.ArgumentParser(description=__doc__)
  parser.add_argument('urls', type=str, nargs='*', action='store',
                      help='Logs')
  return parser


def main(argv):
  parser = GetParser()
  options = parser.parse_args(argv)

  ctx = gs.GSContext()
  for base_url in options.urls:
    glob_url = os.path.join(base_url, '*/status.log')
    files = ctx.LS(glob_url)
    if len(files):
      content = ctx.Cat(files[0])
      result = classifier.ClassifyLabJobStatusResult(content.splitlines())
      print('%s: %s' % (base_url, result))
