#!/usr/bin/python
# Copyright (c) 2009 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Update third_party/WebKit using git.

Under the assumption third_party/WebKit is a clone of git.webkit.org,
we can use git commands to make it match the version requested by DEPS.

See http://code.google.com/p/chromium/wiki/UsingWebKitGit for details on
how to use this.
"""

import os
import re
import subprocess
import sys

# The name of the magic branch that lets us know that DEPS is managing
# the update cycle.
MAGIC_GCLIENT_BRANCH = 'refs/heads/gclient'

def RunGit(command):
  """Run a git subcommand, returning its output."""
  proc = subprocess.Popen(['git'] + command, stdout=subprocess.PIPE)
  return proc.communicate()[0].strip()

def GetWebKitRev():
  """Extract the 'webkit_revision' variable out of DEPS."""
  locals = {'Var': lambda _: ''}
  execfile('DEPS', {}, locals)
  return locals['vars']['webkit_revision']

def FindSVNRev(target_rev):
  """Map an SVN revision to a git hash.
  Like 'git svn find-rev' but without the git-svn bits."""

  # We iterate through the commit log looking for "git-svn-id" lines,
  # which contain the SVN revision of that commit.  We can stop once
  # we've found our target (or hit a revision number lower than what
  # we're looking for, indicating not found).

  target_rev = int(target_rev)

  # regexp matching the "commit" line from the log.
  commit_re = re.compile(r'^commit ([a-f\d]{40})$')
  # regexp matching the git-svn line from the log.
  git_svn_re = re.compile(r'^\s+git-svn-id: [^@]+@(\d+) ')

  log = subprocess.Popen(['git', 'log', '--no-color', '--first-parent',
                          '--pretty=medium', 'origin'],
                         stdout=subprocess.PIPE)
  for line in log.stdout:
    match = commit_re.match(line)
    if match:
      commit = match.group(1)
      continue
    match = git_svn_re.match(line)
    if match:
      rev = int(match.group(1))
      if rev <= target_rev:
        log.stdout.close()  # Break pipe.
        if rev == target_rev:
          return commit
        else:
          return None

  print "Error: reached end of log without finding commit info."
  print "Something has likely gone horribly wrong."
  return None

def UpdateGClientBranch(webkit_rev):
  """Update the magic gclient branch to point at |webkit_rev|.

  Returns: true if the branch didn't need changes."""
  target = FindSVNRev(webkit_rev)
  if not target:
    print "r%s not available; fetching." % webkit_rev
    subprocess.check_call(['git', 'fetch'])
    target = FindSVNRev(webkit_rev)
  if not target:
    print "ERROR: Couldn't map r%s to a git revision." % webkit_rev
    sys.exit(1)

  current = RunGit(['show-ref', '--hash', MAGIC_GCLIENT_BRANCH])
  if current == target:
    return False  # No change necessary.

  subprocess.check_call(['git', 'update-ref', '-m', 'gclient sync',
                         MAGIC_GCLIENT_BRANCH, target])
  return True

def UpdateCurrentCheckoutIfAppropriate():
  """Reset the current gclient branch if that's what we have checked out."""
  branch = RunGit(['symbolic-ref', '-q', 'HEAD'])
  if branch != MAGIC_GCLIENT_BRANCH:
    print "We have now updated the 'gclient' branch, but third_party/WebKit"
    print "has some other branch ('%s') checked out." % branch
    print "Run 'git checkout gclient' under third_party/WebKit if you want"
    print "to switch it to the version requested by DEPS."
    return

  if subprocess.call(['git', 'diff-index', '--exit-code', '--shortstat',
                      'HEAD']):
    print "Resetting tree state to new revision."
    subprocess.check_call(['git', 'reset', '--hard'])

def main():
  if not os.path.exists('third_party/WebKit/.git'):
    print "ERROR: third_party/WebKit appears to not be under git control."
    print "See http://code.google.com/p/chromium/wiki/UsingWebKitGit for"
    print "setup instructions."
    return

  webkit_rev = GetWebKitRev()
  print 'Desired revision: r%s.' % webkit_rev
  os.chdir('third_party/WebKit')
  changed = UpdateGClientBranch(webkit_rev)
  if changed:
    UpdateCurrentCheckoutIfAppropriate()
  else:
    print "Already on correct revision."

if __name__ == '__main__':
  main()
