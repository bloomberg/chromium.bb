#!/usr/bin/env python
# Copyright (c) 2011 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Generate a CL to roll webkit to the specified revision number and post
it to Rietveld so that the CL will land automatically if it passes the
commit-queue's checks.
"""

import logging
import optparse
import os
import re
import sys

import find_depot_tools
import scm
import subprocess2


def die_with_error(msg):
  print >> sys.stderr, msg
  sys.exit(1)


def process_deps(path, new_rev):
  """Update webkit_revision to |new_issue|.

  A bit hacky, could it be made better?
  """
  content = open(path).read()
  old_line = r'(\s+)"webkit_revision": "(\d+)",'
  new_line = r'\1"webkit_revision": "%d",' % new_rev
  new_content = re.sub(old_line, new_line, content, 1)
  if new_content == content:
    die_with_error('Failed to update the DEPS file')
  open(path, 'w').write(new_content)


def main():
  tool_dir = os.path.dirname(os.path.abspath(__file__))
  parser = optparse.OptionParser(usage='<new webkit rev>')
  parser.add_option('-v', '--verbose', action='count', default=0)
  options, args = parser.parse_args()
  logging.basicConfig(
      level=
          [logging.WARNING, logging.INFO, logging.DEBUG][
            min(2, options.verbose)])
  if len(args) != 1:
    parser.error('Need only one arg: new webkit revision to roll to.')

  root_dir = os.path.dirname(tool_dir)
  os.chdir(root_dir)

  new_rev = int(args[0])
  msg = 'Roll webkit revision to %s' % new_rev
  print msg

  # Silence the editor.
  os.environ['EDITOR'] = 'true'

  old_branch = scm.GIT.GetBranch(root_dir)
  if old_branch == 'webkit_roll':
    parser.error(
        'Please delete the branch webkit_roll and move to a different branch')
  subprocess2.check_output(
      ['git', 'checkout', '-b', 'webkit_roll', 'origin/trunk'])
  try:
    process_deps(os.path.join(root_dir, 'DEPS'), new_rev)
    commit_msg = msg + '\nTBR=\n'
    subprocess2.check_output(['git', 'commit', '-m', commit_msg, 'DEPS'])
    subprocess2.check_call(['git', 'diff', 'origin/trunk'])
    subprocess2.check_call(['git', 'cl', 'upload', '--use-commit-queue'])
  finally:
    subprocess2.check_output(['git', 'checkout', old_branch])
    subprocess2.check_output(['git', 'branch', '-D', 'webkit_roll'])
  return 0


if __name__ == '__main__':
  sys.exit(main())
