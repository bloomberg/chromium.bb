#!/usr/bin/python
# Copyright (c) 2010 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Generate fake repositories for testing."""

import logging
import os
import re
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


join = os.path.join


def call(*args, **kwargs):
  logging.debug(args[0])
  subprocess.call(*args, **kwargs)


def check_call(*args, **kwargs):
  logging.debug(args[0])
  subprocess.check_call(*args, **kwargs)


def Popen(*args, **kwargs):
  kwargs.setdefault('stdout', subprocess.PIPE)
  kwargs.setdefault('stderr', subprocess.STDOUT)
  logging.debug(args[0])
  return subprocess.Popen(*args, **kwargs)


def commit_svn(repo):
  """Commits the changes and returns the new revision number."""
  # Basic parsing.
  to_add = []
  to_remove = []
  for item in Popen(['svn', 'status'],
                    cwd=repo).communicate()[0].splitlines(False):
    if item[0] == '?':
      to_add.append(item[8:])
    elif item[0] == '!':
      to_remove.append(item[8:])
  if to_add:
    check_call(['svn', 'add', '--no-auto-props', '-q'] + to_add, cwd=repo)
  if to_remove:
    check_call(['svn', 'remove', '-q'] + to_remove, cwd=repo)
  out = Popen(['svn', 'commit', repo, '-m', 'foo', '--non-interactive',
                '--no-auth-cache', '--username', 'user1', '--password', 'foo'],
              cwd=repo).communicate()[0]
  rev = re.search(r'revision (\d+).', out).group(1)
  st = Popen(['svn', 'status'], cwd=repo).communicate()[0]
  assert len(st) == 0, st
  logging.debug('At revision %s' % rev)
  return rev


def commit_git(repo):
  """Commits the changes and returns the new hash."""
  check_call(['git', 'add', '-A', '-f'], cwd=repo)
  out = Popen(['git', 'commit', '-m', 'foo'], cwd=repo).communicate()[0]
  rev = re.search(r'^\[.*? ([a-f\d]+)\] ', out).group(1)
  logging.debug('At revision %s' % rev)
  return rev


class FakeRepos(object):
  """Generate both svn and git repositories to test gclient functionality.

  Many DEPS functionalities need to be tested: Var, File, From, deps_os, hooks,
  use_relative_paths.

  And types of dependencies: Relative urls, Full urls, both svn and git."""

  def __init__(self, trial_dir, leak, local_only):
    self.trial_dir = trial_dir
    self.repos_dir = os.path.join(self.trial_dir, 'repos')
    self.git_root = join(self.repos_dir, 'git')
    self.svn_root = join(self.repos_dir, 'svn_checkout')
    self.leak = leak
    self.local_only = local_only
    self.svnserve = []
    self.gitdaemon = []
    addKill()
    rmtree(self.trial_dir)
    os.mkdir(self.trial_dir)
    os.mkdir(self.repos_dir)
    # Format is [ None, tree, tree, ...]
    self.svn_revs = [None]
    # Format is { repo: [ (hash, tree), (hash, tree), ... ], ... }
    self.git_hashes = {}

  def setUp(self):
    self.setUpSVN()
    self.setUpGIT()

  def tearDown(self):
    for i in self.svnserve:
      logging.debug('Killing svnserve pid %s' % i.pid)
      i.kill()
    for i in self.gitdaemon:
      logging.debug('Killing git-daemon pid %s' % i.pid)
      i.kill()
    if not self.leak:
      logging.debug('Removing %s' % self.trial_dir)
      rmtree(self.trial_dir)

  def _genTree(self, root, tree_dict):
    """For a dictionary of file contents, generate a filesystem."""
    if not os.path.isdir(root):
      os.makedirs(root)
    for (k, v) in tree_dict.iteritems():
      k_os = k.replace('/', os.sep)
      k_arr = k_os.split(os.sep)
      if len(k_arr) > 1:
        p = os.sep.join([root] + k_arr[:-1])
        if not os.path.isdir(p):
          os.makedirs(p)
      if v is None:
        os.remove(join(root, k))
      else:
        write(join(root, k), v)

  def setUpSVN(self):
    """Creates subversion repositories and start the servers."""
    assert not self.svnserve
    root = join(self.repos_dir, 'svn')
    rmtree(root)
    check_call(['svnadmin', 'create', root])
    write(join(root, 'conf', 'svnserve.conf'),
        '[general]\n'
        'anon-access = read\n'
        'auth-access = write\n'
        'password-db = passwd\n')
    write(join(root, 'conf', 'passwd'),
        '[users]\n'
        'user1 = foo\n'
        'user2 = bar\n')

    # Start the daemon.
    cmd = ['svnserve', '-d', '--foreground', '-r', self.repos_dir]
    if self.local_only:
      cmd.append('--listen-host=127.0.0.1')
    logging.debug(cmd)
    self.svnserve.append(Popen(cmd, cwd=root))
    self.populateSvn()

  def populateSvn(self):
    """Creates a few revisions of changes including DEPS files."""
    # Repos
    check_call(['svn', 'checkout', 'svn://127.0.0.1/svn', self.svn_root, '-q',
                '--non-interactive', '--no-auth-cache',
                '--username', 'user1', '--password', 'foo'])
    assert os.path.isdir(join(self.svn_root, '.svn'))
    def file_system(rev, DEPS):
      fs = {
        'origin': 'svn@%(rev)d\n',
        'trunk/origin': 'svn/trunk@%(rev)d\n',
        'trunk/src/origin': 'svn/trunk/src@%(rev)d\n',
        'trunk/src/third_party/origin': 'svn/trunk/src/third_party@%(rev)d\n',
        'trunk/other/origin': 'src/trunk/other@%(rev)d\n',
        'trunk/third_party/origin': 'svn/trunk/third_party@%(rev)d\n',
        'trunk/third_party/foo/origin': 'svn/trunk/third_party/foo@%(rev)d\n',
        'trunk/third_party/prout/origin': 'svn/trunk/third_party/foo@%(rev)d\n',
      }
      for k in fs.iterkeys():
        fs[k] = fs[k] % { 'rev': rev }
      fs['trunk/src/DEPS'] = DEPS
      return fs

    # Testing:
    # - dependency disapear
    # - dependency renamed
    # - versioned and unversioned reference
    # - relative and full reference
    # - deps_os
    # - var
    # - hooks
    # TODO(maruel):
    # - File
    # - $matching_files
    # - use_relative_paths
    self._commit_svn(file_system(1, """
vars = {
  'DummyVariable': 'third_party',
}
deps = {
  'src/other': 'svn://%(host)s/svn/trunk/other',
  'src/third_party/fpp': '/trunk/' + Var('DummyVariable') + '/foo',
}
deps_os = {
  'mac': {
    'src/third_party/prout': '/trunk/third_party/prout',
  },
}""" % { 'host': '127.0.0.1' }))

    self._commit_svn(file_system(2, """
deps = {
  'src/other': 'svn://%(host)s/svn/trunk/other',
  'src/third_party/foo': '/trunk/third_party/foo@1',
}
# I think this is wrong to have the hooks run from the base of the gclient
# checkout. It's maybe a bit too late to change that behavior.
hooks = [
  {
    'pattern': '.',
    'action': ['python', '-c',
               'open(\\'src/hooked1\\', \\'w\\').write(\\'hooked1\\')'],
  },
  {
    # Should not be run.
    'pattern': 'nonexistent',
    'action': ['python', '-c',
               'open(\\'src/hooked2\\', \\'w\\').write(\\'hooked2\\')'],
  },
]
""" % { 'host': '127.0.0.1' }))

  def setUpGIT(self):
    """Creates git repositories and start the servers."""
    assert not self.gitdaemon
    rmtree(self.git_root)
    for repo in ['repo_%d' % r for r in range(1, 5)]:
      check_call(['git', 'init', '-q', join(self.git_root, repo)])
      self.git_hashes[repo] = []

    # Testing:
    # - dependency disapear
    # - dependency renamed
    # - versioned and unversioned reference
    # - relative and full reference
    # - deps_os
    # - var
    # - hooks
    # TODO(maruel):
    # - File
    # - $matching_files
    # - use_relative_paths
    self._commit_git('repo_1', {
      'DEPS': """
vars = {
  'DummyVariable': 'repo',
}
deps = {
  'src/repo2': 'git://%(host)s/git/repo_2',
  'src/repo2/repo3': '/' + Var('DummyVariable') + '_3',
}
deps_os = {
  'mac': {
    'src/repo4': '/repo_4',
  },
}""" % { 'host': '127.0.0.1' },
      'origin': 'git/repo_1@1\n',
    })

    self._commit_git('repo_2', {
      'origin': "git/repo_2@1\n"
    })

    self._commit_git('repo_2', {
      'origin': "git/repo_2@2\n"
    })

    self._commit_git('repo_3', {
      'origin': "git/repo_3@1\n"
    })

    self._commit_git('repo_3', {
      'origin': "git/repo_3@2\n"
    })

    self._commit_git('repo_4', {
      'origin': "git/repo_4@1\n"
    })

    self._commit_git('repo_4', {
      'origin': "git/repo_4@2\n"
    })

    self._commit_git('repo_1', {
      'DEPS': """
deps = {
  'src/repo2': 'git://%(host)s/git/repo_2@%(hash)s',
  'src/repo2/repo_renamed': '/repo_3',
}
# I think this is wrong to have the hooks run from the base of the gclient
# checkout. It's maybe a bit too late to change that behavior.
hooks = [
  {
    'pattern': '.',
    'action': ['python', '-c',
               'open(\\'src/hooked1\\', \\'w\\').write(\\'hooked1\\')'],
  },
  {
    # Should not be run.
    'pattern': 'nonexistent',
    'action': ['python', '-c',
               'open(\\'src/hooked2\\', \\'w\\').write(\\'hooked2\\')'],
  },
]
""" % { 'host': '127.0.0.1', 'hash': self.git_hashes['repo_2'][0][0] },
      'origin': "git/repo_1@2\n"
    })

    # Start the daemon.
    cmd = ['git', 'daemon', '--export-all', '--base-path=' + self.repos_dir]
    if self.local_only:
      cmd.append('--listen=127.0.0.1')
    logging.debug(cmd)
    self.gitdaemon.append(Popen(cmd, cwd=self.repos_dir))

  def _commit_svn(self, tree):
    self._genTree(self.svn_root, tree)
    commit_svn(self.svn_root)
    if self.svn_revs and self.svn_revs[-1]:
      new_tree = self.svn_revs[-1].copy()
      new_tree.update(tree)
    else:
      new_tree = tree.copy()
    self.svn_revs.append(new_tree)

  def _commit_git(self, repo, tree):
    repo_root = join(self.git_root, repo)
    self._genTree(repo_root, tree)
    hash = commit_git(repo_root)
    if self.git_hashes[repo]:
      new_tree = self.git_hashes[repo][-1][1].copy()
      new_tree.update(tree)
    else:
      new_tree = tree.copy()
    self.git_hashes[repo].append((hash, new_tree))


def main(argv):
  leak = '-l' in argv
  if leak:
    argv.remove('-l')
  verbose = '-v' in argv
  if verbose:
    logging.basicConfig(level=logging.DEBUG)
    argv.remove('-v')
  assert len(argv) == 1, argv
  trial_dir = os.path.join(os.path.dirname(os.path.abspath(__file__)),
                           '_trial')
  print 'Using %s' % trial_dir
  fake = FakeRepos(trial_dir, leak, True)
  try:
    fake.setUp()
    print('Fake setup, press enter to quit or Ctrl-C to keep the checkouts.')
    sys.stdin.readline()
  except KeyboardInterrupt:
    fake.leak = True
  finally:
    fake.tearDown()
  return 0


if __name__ == '__main__':
  sys.exit(main(sys.argv))
