# -*- coding: utf-8 -*-
# Copyright 2018 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Find a project path from this chromiumos checkouts manifest file.

Uses both a project name and a branch name to search the manifest file for a
matching path entry, printing the path to stdout if found and raising an
exception if no match can be found.
"""

from __future__ import print_function

import sys

from chromite.lib import commandline
from chromite.lib import repo_manifest
from chromite.lib import repo_util


assert sys.version_info >= (3, 6), 'This module requires Python 3.6+'


def get_parser():
  """Creates the argparse parser.

  Returns:
    commandline.ArgumentParser: The argument parser.
  """
  parser = commandline.ArgumentParser(description=__doc__)
  parser.add_argument('--manifest-file', type='path',
                      help='File path to a manifest to search.')
  parser.add_argument('--project', required=True,
                      help='The project to search for.')
  parser.add_argument('--branch', default='master',
                      help='The branch to search for.')
  return parser



def main(argv):
  parser = get_parser()
  options = parser.parse_args(argv)
  options.Freeze()
  if options.manifest_file:
    manifest = repo_manifest.Manifest.FromFile(options.manifest_file)
  else:
    manifest = repo_util.Repository.MustFind(__file__).Manifest()
  print(manifest.GetUniqueProject(options.project, options.branch).Path())
