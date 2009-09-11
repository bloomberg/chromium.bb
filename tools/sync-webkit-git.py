#!/usr/bin/python
# Copyright (c) 2009 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Update third_party/WebKit using git.

Under the assumption third_party/WebKit is a clone of git.webkit.org,
we can use git commands to make it match the version requested by DEPS.

To use this:
1) rm -rf third_party/WebKit
2) git clone git://git.webkit.org/WebKit.git third_party/WebKit
3) edit your .gclient "custom_deps" section to exclude components underneath
   third_party/WebKit:
     "src/third_party/WebKit/LayoutTests": None,
     "src/third_party/WebKit/JavaScriptCore": None,
     "src/third_party/WebKit/WebCore": None,
4) run ./tools/sync-webkit-git.py now, and again whenever you run gclient
   sync.

FAQ:
Q. Why not add this functionality to gclient itself?
A. DEPS actually specifies to only pull some subdirectories of
   third_party/WebKit.  So even if gclient supported git, we'd still need
   to special-case this.
"""

import os
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

def FindSVNRev(rev):
  """Map an SVN revision to a git hash.
  Like 'git svn find-rev' but without the git-svn bits."""
  return RunGit(['rev-list', '-n', '1', '--grep=^git-svn-id: .*@%s' % rev,
                 'origin'])

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
    print "Directory has some other branch ('%s') checked out." % branch
    print "Run 'git checkout gclient' to put this under control of gclient."
    return

  if subprocess.call(['git', 'diff-index', '--exit-code', '--shortstat',
	              'HEAD']):
    print "Resetting tree state to new revision."
    subprocess.check_call(['git', 'reset', '--hard'])

def main():
  webkit_rev = GetWebKitRev()
  print 'Desired revision: r%s.' % webkit_rev
  os.chdir('third_party/WebKit')
  changed = UpdateGClientBranch(webkit_rev)
  if changed:
    UpdateCurrentCheckoutIfAppropriate()

if __name__ == '__main__':
  main()
