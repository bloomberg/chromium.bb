#!/usr/bin/python
# Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import optparse
import os
import sys

from chromite.buildbot import constants
from chromite.lib import cros_build_lib


# A set of the command-line options that should not be passed along to the
# gerrit create-project command. See the GetParser docstring for more details.
LOCAL_OPTIONS = set(['dry_run'])


def GetParser():
  """Create a parser for parsing options from the command line.

  This parser is a little fancier than most because it is scraped dynamically
  by the main function to generate the gerrit invocation. The 'dest' option
  specifies the name of the command-line option to Gerrit. If the value passed
  to the 'dest' option is a string, it will be used as the value of the
  command-line option.
  """
  parser = optparse.OptionParser(usage='%prog project-name [options]')

  parser.add_option(
      '--dry-run', default=False, action='store_true', dest='dry_run',
      help='Don\'t actually create the project. Just output what the '
           'gerrit invocation would\'ve been.')
  parser.add_option(
      '--branch', default='master', action='store',
      help='Set the branch that symbolic HEAD will point at; this is typically '
           'master.  Note the branch doesn\'t have to exist, and short form '
           'needs to be used (no refs/heads).')
  parser.add_option(
      '--description', default=None, action='store',
      help='Description of the project being created.')
  parser.add_option(
      '--empty-commit', default=False, action='store_true',
      help='Inject an empty commit into the newly created project\'s '
           'branch; this is usually not desired for third party projects.')
  parser.add_option(
      '--owner', default=None, action='store',
      help='List of groups to grant owner/admin rights to for this project; '
           'see http://goo.gl/DmVzs for docs.')
  parser.add_option(
      '--parent', default=None, action='store',
      help='Set the project\'s parent. If none is specified, the nearest '
           'permission project in the hierarchy will be used. If specified, '
           'the project must exist. The parent doesn\'t have to be a '
           'permission only project. This can be changed later using '
           'gerrit set-project-parent.')
  parser.add_option(
      '--permissions-only', default=False, action='store_true',
      help='If specified, this project is being created purely to attach ACLs '
           'to, and have other projects inherit from.')
  parser.add_option(
      '--require-change-id', default=False, action='store_true',
      help='If specified, a CL cannot be uploaded without a proper Change-Id: '
           'git footer.')
  parser.add_option(
      '--submit-type', default='cherry-pick',
      choices=('cherry-pick', 'merge-if-necessary', 'merge-always',
               'fast-forward-only'),
      help='The method used by Gerrit to merge submitted changes. Chrome '
           'OS and the Chrome OS commit queue only support the cherry-pick '
           'method. For docs, see http://goo.gl/Z1bXm')
  parser.add_option(
      '--disable-content-merge', default=True, action='store_false',
      dest='use_content_merge',
      help='Content merging is best known as "automatically resolve file level '
           'conflicts". If disabled, then Gerrit will always flag a conflict '
           'whenever two unrelated changes affect the same file.')
  parser.add_option(
      '--use-contributor-agreements', default=False, action='store_true',
      help='Require contributor agreements before commit/merge. This is not '
           'currently used in Chrome OS and may not work as expected.')
  parser.add_option(
      '--use-signed-off-by', default=False, action='store_true',
      help='If enabled, require Signed-off-by in git footers. See '
           'http://goo.gl/A1n59 for details. Typically used in kernel '
           'projects.')
  return parser


def FindParent(host, port, name):
  """Find the parent of a given project."""
  lines = cros_build_lib.RunCommandCaptureOutput(
      ['ssh', host, '-p', port, 'gerrit', 'ls-projects', '--type',
       'PERMISSIONS'], shell=False).output
  # Normalize the output to protect ourselves against any gerrit bugs.
  projects = [os.path.normpath(x) for x in lines.splitlines() if x]
  # Add a '/' to the project so that we consider matches at the directory
  # level, rather than intersecting 'clank' against 'clank-bot'.
  scored = sorted((x for x in projects if name.startswith(x + '/')),
                  key=len)
  if scored:
    # Use the project with the longest common prefix.
    return scored[-1]

  # Since we only handle chromeos and chromiumos projects, we should always
  # find a valid parent project. Raise an error if this isn't the case.
  raise AssertionError('Missing parent project for %s' % name)


def main(argv):
  """The main function."""
  parser = GetParser()
  options, args = parser.parse_args(argv)

  # Verify the project is there.
  if len(args) < 1:
    parser.error('Please specify a project')
  if len(args) > 1:
    parser.error('Too many arguments')

  # Verify the project and calculate the hostname.
  name, = args
  if name.startswith('chromeos/'):
    host, port = constants.GERRIT_INT_HOST, constants.GERRIT_INT_PORT
  elif name.startswith('chromiumos/'):
    host, port = constants.GERRIT_HOST, constants.GERRIT_PORT
  else:
    parser.error('Project must be a chromeos/ or chromiumos/ project.')

  # If no parent was specified, search for a parent.
  if options.parent is None:
    options.parent = FindParent(host, port, name)

  cmd = ['ssh', host, '-p', port, 'gerrit', 'create-project', '--name', name]

  # --submit-type has to be in caps for gerrit to be happy; we do the
  # conversion on the user's behalf; same for converting '-' to '_'.
  options.submit_type = options.submit_type.upper().replace('-', '_')
  for option in parser.option_list:
    if option.dest and option.dest not in LOCAL_OPTIONS:
      val = getattr(options, option.dest)
      if val:
        cmd.append('--%s' % option.dest.replace('_', '-'))
        if isinstance(val, basestring):
          cmd.append(val)

  print 'Command is:\n  %s' % ' '.join(map(repr, cmd))
  if not options.dry_run:
    cros_build_lib.RunCommand(cmd, shell=False)
    print 'Created %s' % (name,)
  return 0


if __name__ == '__main__':
  sys.exit(main(sys.argv[1:]))
