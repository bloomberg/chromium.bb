# -*- coding: utf-8 -*-
# Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Script for dumping build config contents."""

from __future__ import print_function

import sys

from chromite.config import chromeos_config
from chromite.lib import config_lib
from chromite.lib import constants
from chromite.lib import commandline

def GetParser():
  """Creates the argparse parser."""
  parser = commandline.ArgumentParser(description=__doc__)

  parser.add_argument('-f', '--full', action='store_true', default=False,
                      help='Dump fully expanded configs.')
  parser.add_argument('-c', '--csv', action='store_true', default=False,
                      help='Dump fully expanded configs as CSV.')
  parser.add_argument('-u', '--update_config', action='store_true',
                      default=False, help='Update the site config json dump.')
  parser.add_argument('-b', '--builder', type=str, default=None,
                      help='Single config to dump.')

  return parser

def main(argv):
  parser = GetParser()
  options = parser.parse_args(argv)

  site_config = chromeos_config.GetConfig()

  filehandle = sys.stdout

  if options.update_config:
    filehandle = open(constants.CHROMEOS_CONFIG_FILE, 'w')

  if options.builder:
    if options.builder not in site_config:
      raise Exception('%s: Not a valid build config.' % options.builder)
    filehandle.write(config_lib.PrettyJsonDict(site_config[options.builder]))
  elif options.full:
    filehandle.write(site_config.DumpExpandedConfigToString())
  elif options.csv:
    filehandle.write(site_config.DumpConfigCsv())
  else:
    filehandle.write(site_config.SaveConfigToString())

  if options.update_config:
    filehandle.close()
