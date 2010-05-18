#!/usr/bin/python
# Copyright (c) 2010 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Generate fake repositories for testing."""

import os
import shutil
import subprocess
import sys


def addKill():
  """Add kill() method to subprocess.Popen for python <2.6"""
  if getattr(subprocess.Popen, 'kill', None):
    return
  if sys.platform.startswith('win'):
    def kill_win(process):
      import win32process
      return win32process.TerminateProcess(process._handle, -1)
    subprocess.kill = kill_win
  else:
    def kill_nix(process):
      import signal
      return os.kill(process.pid, signal.SIGKILL)
    subprocess.kill = kill_nix


def rmtree(path):
  """Delete a directory."""
  if os.path.exists(path):
    shutil.rmtree(path)


def write(path, content):
  f = open(path, 'wb')
  f.write(content)
  f.close()


class FakeRepos(object):
  def __init__(self, trial_dir, leak, local_only):
    self.trial_dir = trial_dir
    self.repos_dir = os.path.join(self.trial_dir, 'repos')
    self.leak = leak
    self.local_only = local_only
    self.svnserve = []
    self.gitdaemon = []
    addKill()
    rmtree(self.trial_dir)
    os.mkdir(self.trial_dir)
    os.mkdir(self.repos_dir)

  def setUp(self):
    self.setUpSVN()
    self.setUpGIT()

  def tearDown(self):
    for i in self.svnserve:
      i.kill()
    for i in self.gitdaemon:
      i.kill()
    if not self.leak:
      rmtree(self.trial_dir)

  def setUpSVN(self):
    """Creates subversion repositories and start the servers."""
    assert not self.svnserve
    join = os.path.join
    root = join(self.repos_dir, 'svn')
    rmtree(root)
    subprocess.check_call(['svnadmin', 'create', root])
    write(join(root, 'conf', 'svnserve.conf'),
        '[general]\n'
        'anon-access = read\n'
        'auth-access = write\n'
        'password-db = passwd\n')
    write(join(root, 'conf', 'passwd'),
        '[users]\n'
        'user1 = foo\n'
        'user2 = bar\n')

    # Repos
    repo = join(self.repos_dir, 'svn_import')
    rmtree(repo)
    os.mkdir(repo)
    os.mkdir(join(repo, 'trunk'))
    os.mkdir(join(repo, 'trunk', 'src'))
    write(join(repo, 'trunk', 'src', 'DEPS'), """
# Smoke test DEPS file.
# Many DEPS functionalities need to be tested:
#  Var
#  File
#  From
#  deps_os
#  hooks
#  use_relative_paths
#
# Types of dependencies:
#  Relative urls
#  Full urls
#  svn
#  git

deps = {
 'src/other': 'svn://%(host)s/svn/trunk/other',
 'src/third_party': '/trunk/third_party',
}

deps_os = {
 'mac': 'repo_4'
}
""" % {
    'host': 'localhost',
})
    write(join(repo, 'trunk', 'src', 'origin'), "svn/trunk/src")
    os.mkdir(join(repo, 'trunk', 'other'))
    write(join(repo, 'trunk', 'other', 'origin'), "svn/trunk/other")
    os.mkdir(join(repo, 'trunk', 'third_party'))
    write(join(repo, 'trunk', 'third_party', 'origin'), "svn/trunk/third_party")

    # Start the daemon.
    cmd = ['svnserve', '-d', '--foreground', '-r', self.repos_dir]
    if self.local_only:
      cmd.append('--listen-host=127.0.0.1')
    self.svnserve.append(subprocess.Popen(cmd, cwd=root))

    # Import the repo.
    subprocess.check_call(['svn', 'import', repo,
      'svn://127.0.0.1/svn', '-m', 'foo', '-q',
      '--no-auto-props', '--non-interactive', '--no-auth-cache',
      '--username', 'user1', '--password', 'foo'])

  def setUpGIT(self):
    """Creates git repositories and start the servers."""
    assert not self.gitdaemon
    join = os.path.join
    root = join(self.repos_dir, 'git')
    rmtree(root)
    os.mkdir(root)
    # Repo 1
    repo = join(root, 'repo_1')
    subprocess.check_call(['git', 'init', '-q', repo])
    write(join(repo, 'DEPS'), """
# Smoke test DEPS file.
# Many DEPS functionalities need to be tested:
#  Var
#  File
#  From
#  deps_os
#  hooks
#  use_relative_paths
#
# Types of dependencies:
#  Relative urls
#  Full urls
#  svn
#  git

deps = {
 'repo2': 'git://%(host)s/git/repo_2',
 'repo2/repo3': '/repo_3',
}

deps_os = {
 'mac': 'repo_4'
}
""" % {
    'host': 'localhost',
})
    write(join(repo, 'origin'), "git/repo_1")
    subprocess.check_call(['git', 'add', '-A', '-f'], cwd=repo)
    subprocess.check_call(['git', 'commit', '-q', '-m', 'foo'], cwd=repo)

    # Repo 2
    repo = join(root, 'repo_2')
    subprocess.check_call(['git', 'init', '-q', repo])
    write(join(repo, 'origin'), "git/repo_2")
    subprocess.check_call(['git', 'add', '-A', '-f'], cwd=repo)
    subprocess.check_call(['git', 'commit', '-q', '-m', 'foo'], cwd=repo)

    # Repo 3
    repo = join(root, 'repo_3')
    subprocess.check_call(['git', 'init', '-q', repo])
    write(join(repo, 'origin'), "git/repo_3")
    subprocess.check_call(['git', 'add', '-A', '-f'], cwd=repo)
    subprocess.check_call(['git', 'commit', '-q', '-m', 'foo'], cwd=repo)

    # Start the daemon.
    cmd = ['git', 'daemon', '--export-all', '--base-path=' + self.repos_dir]
    if self.local_only:
      cmd.append('--listen=127.0.0.1')
    self.gitdaemon.append(subprocess.Popen(cmd, cwd=self.repos_dir,
                                           stderr=subprocess.PIPE))

if __name__ == '__main__':
  fake = FakeRepos(os.path.dirname(os.path.abspath(__file__)), False)
  try:
    fake.setUp()
    sys.stdin.readline()
  finally:
    fake.tearDown()
