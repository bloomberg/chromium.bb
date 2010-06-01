#!/usr/bin/python
# Copyright (c) 2010 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Generate fake repositories for testing."""

import atexit
import errno
import logging
import os
import pprint
import re
import stat
import subprocess
import sys
import time
import unittest


## Utility functions


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


def rmtree(*path):
  """Recursively removes a directory, even if it's marked read-only.

  Remove the directory located at *path, if it exists.

  shutil.rmtree() doesn't work on Windows if any of the files or directories
  are read-only, which svn repositories and some .svn files are.  We need to
  be able to force the files to be writable (i.e., deletable) as we traverse
  the tree.

  Even with all this, Windows still sometimes fails to delete a file, citing
  a permission error (maybe something to do with antivirus scans or disk
  indexing).  The best suggestion any of the user forums had was to wait a
  bit and try again, so we do that too.  It's hand-waving, but sometimes it
  works. :/
  """
  file_path = os.path.join(*path)
  if not os.path.exists(file_path):
    return

  def RemoveWithRetry_win(rmfunc, path):
    os.chmod(path, stat.S_IWRITE)
    if win32_api_avail:
      win32api.SetFileAttributes(path, win32con.FILE_ATTRIBUTE_NORMAL)
    try:
      return rmfunc(path)
    except EnvironmentError, e:
      if e.errno != errno.EACCES:
        raise
      print 'Failed to delete %s: trying again' % repr(path)
      time.sleep(0.1)
      return rmfunc(path)

  def RemoveWithRetry_non_win(rmfunc, path):
    if os.path.islink(path):
      return os.remove(path)
    else:
      return rmfunc(path)

  win32_api_avail = False
  remove_with_retry = None
  if sys.platform.startswith('win'):
    # Some people don't have the APIs installed. In that case we'll do without.
    try:
      win32api = __import__('win32api')
      win32con = __import__('win32con')
      win32_api_avail = True
    except ImportError:
      pass
    remove_with_retry = RemoveWithRetry_win
  else:
    remove_with_retry = RemoveWithRetry_non_win

  for root, dirs, files in os.walk(file_path, topdown=False):
    # For POSIX:  making the directory writable guarantees removability.
    # Windows will ignore the non-read-only bits in the chmod value.
    os.chmod(root, 0770)
    for name in files:
      remove_with_retry(os.remove, os.path.join(root, name))
    for name in dirs:
      remove_with_retry(os.rmdir, os.path.join(root, name))

  remove_with_retry(os.rmdir, file_path)


def write(path, content):
  f = open(path, 'wb')
  f.write(content)
  f.close()


join = os.path.join


def check_call(*args, **kwargs):
  logging.debug(args[0])
  subprocess.check_call(*args, **kwargs)


def Popen(*args, **kwargs):
  kwargs.setdefault('stdout', subprocess.PIPE)
  kwargs.setdefault('stderr', subprocess.STDOUT)
  logging.debug(args[0])
  return subprocess.Popen(*args, **kwargs)


def read_tree(tree_root):
  """Returns a dict of all the files in a tree. Defaults to self.root_dir."""
  tree = {}
  for root, dirs, files in os.walk(tree_root):
    for d in filter(lambda x: x.startswith('.'), dirs):
      dirs.remove(d)
    for f in [join(root, f) for f in files if not f.startswith('.')]:
      filepath = f[len(tree_root) + 1:].replace(os.sep, '/')
      assert len(filepath), f
      tree[filepath] = open(join(root, f), 'rU').read()
  return tree


def dict_diff(dict1, dict2):
  diff = {}
  for k, v in dict1.iteritems():
    if k not in dict2:
      diff[k] = v
    elif v != dict2[k]:
      diff[k] = (v, dict2[k])
  for k, v in dict2.iteritems():
    if k not in dict1:
      diff[k] = v
  return diff


def mangle_svn_tree(*args):
  result = {}
  for old_root, new_root, tree in args:
    for k, v in tree.iteritems():
      if not k.startswith(old_root):
        continue
      result[join(new_root, k[len(old_root) + 1:]).replace(os.sep, '/')] = v
  return result


def mangle_git_tree(*args):
  result = {}
  for new_root, tree in args:
    for k, v in tree.iteritems():
      result[join(new_root, k)] = v
  return result


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
  check_call(['git', 'commit', '-q', '--message', 'foo'], cwd=repo)
  rev = Popen(['git', 'show-ref', '--head', 'HEAD'],
              cwd=repo).communicate()[0].split(' ', 1)[0]
  logging.debug('At revision %s' % rev)
  return rev


_FAKE_LOADED = False

class FakeRepos(object):
  """Generate both svn and git repositories to test gclient functionality.

  Many DEPS functionalities need to be tested: Var, File, From, deps_os, hooks,
  use_relative_paths.

  And types of dependencies: Relative urls, Full urls, both svn and git."""

  # Should leak the repositories.
  SHOULD_LEAK = False
  # Override if unhappy.
  TRIAL_DIR = None
  # Hostname
  HOST = '127.0.0.1'

  def __init__(self, trial_dir=None, leak=None, host=None):
    global _FAKE_LOADED
    if _FAKE_LOADED:
      raise Exception('You can only start one FakeRepos at a time.')
    _FAKE_LOADED = True
    # Quick hack.
    if '-v' in sys.argv:
      logging.basicConfig(level=logging.DEBUG)
    if '-l' in sys.argv:
      self.SHOULD_LEAK = True
      sys.argv.remove('-l')
    elif leak is not None:
      self.SHOULD_LEAK = leak
    if host:
      self.HOST = host
    if trial_dir:
      self.TRIAL_DIR = trial_dir

    # Format is [ None, tree, tree, ...]
    self.svn_revs = [None]
    # Format is { repo: [ (hash, tree), (hash, tree), ... ], ... }
    self.git_hashes = {}
    self.svnserve = None
    self.gitdaemon = None
    self.common_init = False

  def trial_dir(self):
    if not self.TRIAL_DIR:
      self.TRIAL_DIR = os.path.join(
          os.path.dirname(os.path.abspath(__file__)), '_trial')
    return self.TRIAL_DIR

  def setUp(self):
    """All late initialization comes here.

    Note that it deletes all trial_dir() and not only repos_dir."""
    if not self.common_init:
      self.common_init = True
      self.repos_dir = os.path.join(self.trial_dir(), 'repos')
      self.git_root = join(self.repos_dir, 'git')
      self.svn_root = join(self.repos_dir, 'svn_checkout')
      addKill()
      rmtree(self.trial_dir())
      os.makedirs(self.repos_dir)
      atexit.register(self.tearDown)

  def tearDown(self):
    if self.svnserve:
      logging.debug('Killing svnserve pid %s' % self.svnserve.pid)
      self.svnserve.kill()
      self.svnserve = None
    if self.gitdaemon:
      logging.debug('Killing git-daemon pid %s' % self.gitdaemon.pid)
      self.gitdaemon.kill()
      self.gitdaemon = None
    if not self.SHOULD_LEAK:
      logging.debug('Removing %s' % self.trial_dir())
      rmtree(self.trial_dir())

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
    if self.svnserve:
      return
    self.setUp()
    root = join(self.repos_dir, 'svn')
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
    if self.HOST == '127.0.0.1':
      cmd.append('--listen-host=127.0.0.1')
    logging.debug(cmd)
    self.svnserve = Popen(cmd, cwd=root)
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
}""" % { 'host': self.HOST }))

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
               'open(\\'src/svn_hooked1\\', \\'w\\').write(\\'svn_hooked1\\')'],
  },
  {
    # Should not be run.
    'pattern': 'nonexistent',
    'action': ['python', '-c',
               'open(\\'src/svn_hooked2\\', \\'w\\').write(\\'svn_hooked2\\')'],
  },
]
""" % { 'host': self.HOST }))

  def setUpGIT(self):
    """Creates git repositories and start the servers."""
    if self.gitdaemon:
      return True
    self.setUp()
    if sys.platform == 'win32':
      return False
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
}""" % { 'host': self.HOST },
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
               'open(\\'src/git_hooked1\\', \\'w\\').write(\\'git_hooked1\\')'],
  },
  {
    # Should not be run.
    'pattern': 'nonexistent',
    'action': ['python', '-c',
               'open(\\'src/git_hooked2\\', \\'w\\').write(\\'git_hooked2\\')'],
  },
]
""" % {
        # TODO(maruel): http://crosbug.com/3591 We need to strip the hash.. duh.
        'host': self.HOST,
        'hash': self.git_hashes['repo_2'][0][0][:7]
      },
      'origin': "git/repo_1@2\n"
    })

    # Start the daemon.
    cmd = ['git', 'daemon', '--export-all', '--base-path=' + self.repos_dir]
    if self.HOST == '127.0.0.1':
      cmd.append('--listen=127.0.0.1')
    logging.debug(cmd)
    self.gitdaemon = Popen(cmd, cwd=self.repos_dir)
    return True

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


class FakeReposTestBase(unittest.TestCase):
  """This is vaguely inspired by twisted."""

  # Replace this in your subclass.
  CLASS_ROOT_DIR = None

  # static FakeRepos instance.
  FAKE_REPOS = FakeRepos()

  def setUp(self):
    unittest.TestCase.setUp(self)
    self.FAKE_REPOS.setUp()

    # Remove left overs and start fresh.
    if not self.CLASS_ROOT_DIR:
      self.CLASS_ROOT_DIR = join(self.FAKE_REPOS.trial_dir(), 'smoke')
    self.root_dir = join(self.CLASS_ROOT_DIR, self.id())
    rmtree(self.root_dir)
    os.makedirs(self.root_dir)
    self.svn_base = 'svn://%s/svn/' % self.FAKE_REPOS.HOST
    self.git_base = 'git://%s/git/' % self.FAKE_REPOS.HOST

  def tearDown(self):
    if not self.FAKE_REPOS.SHOULD_LEAK:
      rmtree(self.root_dir)

  def checkString(self, expected, result):
    """Prints the diffs to ease debugging."""
    if expected != result:
      # Strip the begining
      while expected and result and expected[0] == result[0]:
        expected = expected[1:]
        result = result[1:]
      # The exception trace makes it hard to read so dump it too.
      if '\n' in result:
        print result
    self.assertEquals(expected, result)

  def check(self, expected, results):
    """Checks stdout, stderr, retcode."""
    self.checkString(expected[0], results[0])
    self.checkString(expected[1], results[1])
    self.assertEquals(expected[2], results[2])

  def assertTree(self, tree, tree_root=None):
    """Diff the checkout tree with a dict."""
    if not tree_root:
      tree_root = self.root_dir
    actual = read_tree(tree_root)
    diff = dict_diff(tree, actual)
    if diff:
      logging.debug('Actual %s\n%s' % (tree_root, pprint.pformat(actual)))
      logging.debug('Expected\n%s' % pprint.pformat(tree))
      logging.debug('Diff\n%s' % pprint.pformat(diff))
      self.assertEquals(diff, [])


def main(argv):
  fake = FakeRepos()
  print 'Using %s' % fake.trial_dir()
  try:
    fake.setUp()
    print('Fake setup, press enter to quit or Ctrl-C to keep the checkouts.')
    sys.stdin.readline()
  except KeyboardInterrupt:
    fake.SHOULD_LEAK = True
  return 0


if __name__ == '__main__':
  sys.exit(main(sys.argv))
