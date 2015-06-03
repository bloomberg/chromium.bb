# Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Script for dumping build config contents."""

from __future__ import print_function

from chromite.cbuildbot import chromeos_config
from chromite.lib import commandline

def GetParser():
  """Creates the argparse parser."""
  parser = commandline.ArgumentParser(description=__doc__)

  parser.add_argument('-f', '--full', action='store_true', default=False,
                      help='Dump fully expanded configs.')

  return parser

def main(argv):
  parser = GetParser()
  options = parser.parse_args(argv)

  site_config = chromeos_config.GetConfig()

  if options.full:
    print(site_config.DumpExpandedConfigToString())
  else:
    print(site_config.SaveConfigToString())
