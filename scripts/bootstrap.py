# Copyright 2016 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Bootstrap for cbuildbot.

This script is intended to checkout chromite on the branch specified by -b or
--branch (as normally accepted by cbuildbot), and then invoke cbuildbot. Most
arguments are not parsed, only passed along. If a branch is not specified, this
script will use 'master'.

Among other things, this allows us to invoke build configs that exist on a given
branch, but not on TOT.
"""

from __future__ import print_function

import os

from chromite.cbuildbot import repository
from chromite.lib import config_lib
from chromite.lib import cros_build_lib
from chromite.lib import cros_logging as logging
from chromite.lib import osutils
from chromite.scripts import cbuildbot

def PreParseArguments(argv):
  """Extract the branch name from cbuildbot command line arguments.

  Ignores all arguments, other than the branch name.

  Args:
    argv: The command line arguments to parse.

  Returns:
    Branch as a string ('master' if nothing is specified).
  """
  # Must match cbuildbot._CreateParser().
  parser = cbuildbot.CreateParser()

  # Extract the branch argument, if present, ignore the rest.
  options, _ = parser.parse_args(argv)

  # This option isn't required for cbuildbot, but is for us.
  if not options.buildroot:
    cros_build_lib.Die('--buildroot is a required option.')

  return options


def InitialCheckout(branchname, buildroot, git_cache_dir):
  """Preliminary ChromeOS checkout.

  Perform a complete checkout of ChromeOS on the specified branch. This does NOT
  match what the build needs, but ensures the buildroot both has a 'hot'
  checkout, and is close enough that the branched cbuildbot can successfully get
  the right checkout.

  This checks out full ChromeOS, even if a ChromiumOS build is going to be
  performed. This is because we have no knowledge of the build config to be
  used.

  Args:
    branchname: Name of branch to checkout. None for no branch.
    buildroot: Directory to checkout into.
    git_cache_dir: Directory to use for git cache. None to not use it.
  """
  site_config = config_lib.GetConfig()
  manifest_url = site_config.params['MANIFEST_INT_URL']

  osutils.SafeMakedirs(buildroot)
  repo = repository.RepoRepository(manifest_url, buildroot,
                                   branch=branchname,
                                   git_cache_dir=git_cache_dir)
  repo.Sync()


def RunCbuildbot(buildroot, argv):
  """Start cbuildbot in specified directory with all arguments.

  Args:
    buildroot: Root of ChromeOS checkout to run cbuildbot in.
    argv: All command line arguments to pass as list of strings.

  Returns:
    Return code of cbuildbot as an integer.
  """
  logging.info('Bootstrap cbuildbot in: %s', buildroot)
  cbuildbot_cmd = os.path.join(buildroot, 'chromite', 'bin', 'cbuildbot')
  result = cros_build_lib.RunCommand([cbuildbot_cmd] + argv,
                                     error_code_ok=True,
                                     cwd=buildroot)

  logging.debug('cbuildbot result is: %s', result.returncode)
  return result.returncode


def main(argv):
  """main method of script.

  Args:
    argv: All command line arguments to pass as list of strings.

  Returns:
    Return code of cbuildbot as an integer.
  """
  # Specified branch, or 'master'
  options = PreParseArguments(argv)

  branchname = options.branch
  buildroot = options.buildroot
  git_cache_dir = options.git_cache_dir

  # Get a checkout close enough the branched cbuildbot can handle it.
  InitialCheckout(branchname, buildroot, git_cache_dir)

  # Run cbuildbot inside the full ChromeOS checkout, on the specified branch.
  RunCbuildbot(buildroot, argv)
