# -*- coding: utf-8 -*-
# Copyright 2016 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Try to set up a chromite virtualenv, and print the import path."""

from __future__ import print_function

import sys
import pprint

from chromite.lib import commandline


assert sys.version_info >= (3, 6), 'This module requires Python 3.6+'


def GetParser():
  """Creates the argparse parser."""
  parser = commandline.ArgumentParser(description=__doc__)
  return parser


def main(argv):
  parser = GetParser()
  parser.parse_args(argv)

  print('The import path is:')
  pprint.pprint(sys.path)
