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
from chromite.lib import commandline
from chromite.lib import constants
from chromite.lib import cros_build_lib
from chromite.lib import cros_logging as logging
from chromite.lib import git
from chromite.lib import osutils

def ExtractBranchName(argv):
  """Extract the branch name from cbuildbot command line arguments.

  Ignores all arguments, other than the branch name.

  Args:
    argv: The command line arguments to parse.

  Returns:
    Branch as a string ('master' if nothing is specified).
  """
  # Must match cbuildbot._CreateParser().
  parser = commandline.ArgumentParser(description=__doc__)
  parser.add_argument('-b', '--branch',
                      default='master',
                      help='The manifest branch to test.  The branch to '
                           'check the buildroot out to.')

  # Extract the branch argument, if present, ignore the rest.
  options, _ = parser.parse_known_args(argv)
  return options.branch


def CloneChromiteOnBranch(branch_name, branch_dir):
  """Create a worktree of the current chromite checkout.

  Args:
    branch_name: Name of the branch to checkout, in --branch format, or None.
                 None means use the current branch.
    branch_dir: Empty directory to worktree into.
  """
  assert branch_name
  logging.info('Creating chromite clone of branch %s in %s',
               branch_name, branch_dir)

  reference_repo = os.path.join(constants.CHROMITE_DIR, '.git')
  repository.CloneGitRepo(branch_dir, constants.CHROMITE_URL,
                          reference=reference_repo)
  git.RunGit(branch_dir, ['checkout', branch_name])


def RunCbuildbot(chromite_dir, argv):
  """Start cbuildbot in specified directory with all arguments.

  Args:
    chromite_dir: Root of chromite checkout to run cbuildbot in.
    argv: All command line arguments to pass as list of strings.

  Returns:
    Return code of cbuildbot as an integer.
  """
  logging.info('Bootstrap cbuildbot in: %s', chromite_dir)
  cbuildbot = os.path.join(chromite_dir, 'bin', 'cbuildbot')
  result = cros_build_lib.RunCommand([cbuildbot] + argv,
                                     error_code_ok=True,
                                     cwd=chromite_dir)

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
  branch_name = ExtractBranchName(argv)

  # Run cbuildbot in branched clone of current repo.
  with osutils.TempDir(prefix='bootstrap_branched_chromite-') as branch_dir:
    CloneChromiteOnBranch(branch_name, branch_dir)
    return RunCbuildbot(branch_dir, argv)
