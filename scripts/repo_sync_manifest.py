# -*- coding: utf-8 -*-
# Copyright 2018 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Sync a repo checkout to the given manifest file."""

from __future__ import print_function

import os

from chromite.lib import commandline
from chromite.lib import repo_util


def GetParser():
  """Creates the argparse parser."""
  parser = commandline.ArgumentParser(description=__doc__)
  parser.add_argument('manifest', type='path',
                      help='The manifest file to sync to.')
  parser.add_argument('--repo-root', type='path', default='.',
                      help='Path to the repo root to sync.')
  parser.add_argument('--copy-repo', type='path',
                      help='Path to an existing repo root; the .repo dir '
                           'there will be copied to the target repo root, '
                           'which must not be within an existing root.')
  return parser


def main(argv):
  parser = GetParser()
  options = parser.parse_args(argv)
  options.Freeze()

  # Assert manifest exists.
  os.stat(options.manifest)

  if options.copy_repo:
    copy_repo = repo_util.Repository(options.copy_repo)
    copy_repo.Copy(options.repo_root)

  repo = repo_util.Repository(options.repo_root)

  repo.Sync(manifest_path=options.manifest)
