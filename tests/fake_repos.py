#!/usr/bin/python
# Copyright (c) 2010 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Generate fake repositories for testing."""

import atexit
import datetime
import errno
import logging
import os
import pprint
import re
import socket
import stat
import subprocess
import sys
import tempfile
import time

# trial_dir must be first for non-system libraries.
from tests import trial_dir
import scm

## Utility functions


def kill_pid(pid):
  """Kills a process by its process id."""
  try:
    # Unable to import 'module'
    # pylint: disable=F0401
    import signal
    return os.kill(pid, signal.SIGKILL)
  except ImportError:
    pass


def kill_win(process):
  """Kills a process with its windows handle.

  Has no effect on other platforms.
  """
  try:
    # Unable to import 'module'
    # pylint: disable=F0401
    import win32process
    # Access to a protected member _handle of a client class
    # pylint: disable=W0212
    return win32process.TerminateProcess(process._handle, -1)
  except ImportError:
    pass


def add_kill():
  """Adds kill() method to subprocess.Popen for python <2.6"""
  if hasattr(subprocess.Popen, 'kill'):
    return

  if sys.platform == 'win32':
    subprocess.Popen.kill = kill_win
  else:
    def kill_nix(process):
      return kill_pid(process.pid)
    subprocess.Popen.kill = kill_nix


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


def commit_svn(repo, usr, pwd):
  """Commits the changes and returns the new revision number."""
  to_add = []
  to_remove = []
  for status, filepath in scm.SVN.CaptureStatus(repo):
    if status[0] == '?':
      to_add.append(filepath)
    elif status[0] == '!':
      to_remove.append(filepath)
  if to_add:
    check_call(['svn', 'add', '--no-auto-props', '-q'] + to_add, cwd=repo)
  if to_remove:
    check_call(['svn', 'remove', '-q'] + to_remove, cwd=repo)
  proc = Popen(
      ['svn', 'commit', repo, '-m', 'foo', '--non-interactive',
        '--no-auth-cache',
        '--username', usr, '--password', pwd],
      cwd=repo)
  out, err = proc.communicate()
  match = re.search(r'(\d+)', out)
  if not match:
    raise Exception('Commit failed', out, err, proc.returncode)
  rev = match.group(1)
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

class FakeReposBase(object):
  """Generate both svn and git repositories to test gclient functionality.

  Many DEPS functionalities need to be tested: Var, File, From, deps_os, hooks,
  use_relative_paths.

  And types of dependencies: Relative urls, Full urls, both svn and git.

  populateSvn() and populateGit() need to be implemented by the subclass.
  """
  # Hostname
  NB_GIT_REPOS = 1
  USERS = [
      ('user1@example.com', 'foo'),
      ('user2@example.com', 'bar'),
  ]

  def __init__(self, host=None):
    global _FAKE_LOADED
    if _FAKE_LOADED:
      raise Exception('You can only start one FakeRepos at a time.')
    _FAKE_LOADED = True

    self.trial = trial_dir.TrialDir('repos')
    self.host = host or '127.0.0.1'
    # Format is [ None, tree, tree, ...]
    # i.e. revisions are 1-based.
    self.svn_revs = [None]
    # Format is { repo: [ None, (hash, tree), (hash, tree), ... ], ... }
    # so reference looks like self.git_hashes[repo][rev][0] for hash and
    # self.git_hashes[repo][rev][1] for it's tree snapshot.
    # For consistency with self.svn_revs, it is 1-based too.
    self.git_hashes = {}
    self.svnserve = None
    self.gitdaemon = None
    self.git_pid_file = None
    self.git_root = None
    self.svn_checkout = None
    self.svn_repo = None
    self.git_dirty = False
    self.svn_dirty = False
    self.svn_base = 'svn://%s/svn/' % self.host
    self.git_base = 'git://%s/git/' % self.host
    self.svn_port = 3690
    self.git_port = 9418

  @property
  def root_dir(self):
    return self.trial.root_dir

  def set_up(self):
    """All late initialization comes here."""
    self.cleanup_dirt()
    if not self.root_dir:
      try:
        add_kill()
        # self.root_dir is not set before this call.
        self.trial.set_up()
        self.git_root = join(self.root_dir, 'git')
        self.svn_checkout = join(self.root_dir, 'svn_checkout')
        self.svn_repo = join(self.root_dir, 'svn')
      finally:
        # Registers cleanup.
        atexit.register(self.tear_down)

  def cleanup_dirt(self):
    """For each dirty repository, destroy it."""
    if self.svn_dirty:
      if not self.tear_down_svn():
        logging.error('Using both leaking checkout and svn dirty checkout')
    if self.git_dirty:
      if not self.tear_down_git():
        logging.error('Using both leaking checkout and git dirty checkout')

  def tear_down(self):
    """Kills the servers and delete the directories."""
    self.tear_down_svn()
    self.tear_down_git()
    # This deletes the directories.
    self.trial.tear_down()
    self.trial = None

  def tear_down_svn(self):
    if self.svnserve:
      logging.debug('Killing svnserve pid %s' % self.svnserve.pid)
      self.svnserve.kill()
      self.wait_for_port_to_free(self.svn_port)
      self.svnserve = None
      if not self.trial.SHOULD_LEAK:
        logging.debug('Removing %s' % self.svn_repo)
        rmtree(self.svn_repo)
        logging.debug('Removing %s' % self.svn_checkout)
        rmtree(self.svn_checkout)
      else:
        return False
    return True

  def tear_down_git(self):
    if self.gitdaemon:
      logging.debug('Killing git-daemon pid %s' % self.gitdaemon.pid)
      self.gitdaemon.kill()
      self.gitdaemon = None
      if self.git_pid_file:
        pid = int(self.git_pid_file.read())
        self.git_pid_file.close()
        logging.debug('Killing git daemon pid %s' % pid)
        kill_pid(pid)
        self.git_pid_file = None
      self.wait_for_port_to_free(self.git_port)
      if not self.trial.SHOULD_LEAK:
        logging.debug('Removing %s' % self.git_root)
        rmtree(self.git_root)
      else:
        return False
    return True

  @staticmethod
  def _genTree(root, tree_dict):
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

  def set_up_svn(self):
    """Creates subversion repositories and start the servers."""
    self.set_up()
    if self.svnserve:
      return True
    try:
      check_call(['svnadmin', 'create', self.svn_repo])
    except OSError:
      return False
    write(join(self.svn_repo, 'conf', 'svnserve.conf'),
        '[general]\n'
        'anon-access = read\n'
        'auth-access = write\n'
        'password-db = passwd\n')
    text = '[users]\n'
    text += ''.join('%s = %s\n' % (usr, pwd) for usr, pwd in self.USERS)
    write(join(self.svn_repo, 'conf', 'passwd'), text)

    # Start the daemon.
    cmd = ['svnserve', '-d', '--foreground', '-r', self.root_dir]
    if self.host == '127.0.0.1':
      cmd.append('--listen-host=' + self.host)
    self.check_port_is_free(self.svn_port)
    self.svnserve = Popen(cmd, cwd=self.svn_repo)
    self.wait_for_port_to_bind(self.svn_port, self.svnserve)
    self.populateSvn()
    self.svn_dirty = False
    return True

  def set_up_git(self):
    """Creates git repositories and start the servers."""
    self.set_up()
    if self.gitdaemon:
      return True
    if sys.platform == 'win32':
      return False
    assert self.git_pid_file == None
    for repo in ['repo_%d' % r for r in range(1, self.NB_GIT_REPOS + 1)]:
      check_call(['git', 'init', '-q', join(self.git_root, repo)])
      self.git_hashes[repo] = [None]
    # Unlike svn, populate git before starting the server.
    self.populateGit()
    # Start the daemon.
    self.git_pid_file = tempfile.NamedTemporaryFile()
    cmd = ['git', 'daemon',
        '--export-all',
        '--reuseaddr',
        '--base-path=' + self.root_dir,
        '--pid-file=' + self.git_pid_file.name]
    if self.host == '127.0.0.1':
      cmd.append('--listen=' + self.host)
    self.check_port_is_free(self.git_port)
    self.gitdaemon = Popen(cmd, cwd=self.root_dir)
    self.wait_for_port_to_bind(self.git_port, self.gitdaemon)
    self.git_dirty = False
    return True

  def _commit_svn(self, tree):
    self._genTree(self.svn_checkout, tree)
    commit_svn(self.svn_checkout, self.USERS[0][0], self.USERS[0][1])
    if self.svn_revs and self.svn_revs[-1]:
      new_tree = self.svn_revs[-1].copy()
      new_tree.update(tree)
    else:
      new_tree = tree.copy()
    self.svn_revs.append(new_tree)

  def _commit_git(self, repo, tree):
    repo_root = join(self.git_root, repo)
    self._genTree(repo_root, tree)
    commit_hash = commit_git(repo_root)
    if self.git_hashes[repo][-1]:
      new_tree = self.git_hashes[repo][-1][1].copy()
      new_tree.update(tree)
    else:
      new_tree = tree.copy()
    self.git_hashes[repo].append((commit_hash, new_tree))

  def check_port_is_free(self, port):
    sock = socket.socket()
    try:
      sock.connect((self.host, port))
      # It worked, throw.
      assert False, '%d shouldn\'t be bound' % port
    except EnvironmentError:
      pass
    finally:
      sock.close()

  def wait_for_port_to_bind(self, port, process):
    sock = socket.socket()
    try:
      start = datetime.datetime.utcnow()
      maxdelay = datetime.timedelta(seconds=30)
      while (datetime.datetime.utcnow() - start) < maxdelay:
        try:
          sock.connect((self.host, port))
          logging.debug('%d is now bound' % port)
          return
        except EnvironmentError:
          pass
        logging.debug('%d is still not bound' % port)
    finally:
      sock.close()
    # The process failed to bind. Kill it and dump its ouput.
    process.kill()
    logging.error('%s' % process.communicate()[0])
    assert False, '%d is still not bound' % port

  def wait_for_port_to_free(self, port):
    start = datetime.datetime.utcnow()
    maxdelay = datetime.timedelta(seconds=30)
    while (datetime.datetime.utcnow() - start) < maxdelay:
      try:
        sock = socket.socket()
        sock.connect((self.host, port))
        logging.debug('%d was bound, waiting to free' % port)
      except EnvironmentError:
        logging.debug('%d now free' % port)
        return
      finally:
        sock.close()
    assert False, '%d is still bound' % port

  def populateSvn(self):
    raise NotImplementedError()

  def populateGit(self):
    raise NotImplementedError()


class FakeRepos(FakeReposBase):
  """Implements populateSvn() and populateGit()."""
  NB_GIT_REPOS = 4

  def populateSvn(self):
    """Creates a few revisions of changes including DEPS files."""
    # Repos
    check_call(['svn', 'checkout', self.svn_base, self.svn_checkout,
                '-q', '--non-interactive', '--no-auth-cache',
                '--username', self.USERS[0][0], '--password', self.USERS[0][1]])
    assert os.path.isdir(join(self.svn_checkout, '.svn'))
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
    # - From
    # - File
    # TODO(maruel):
    # - $matching_files
    # - use_relative_paths
    fs = file_system(1, """
vars = {
  'DummyVariable': 'third_party',
}
deps = {
  'src/other': '%(svn_base)strunk/other@1',
  'src/third_party/fpp': '/trunk/' + Var('DummyVariable') + '/foo',
}
deps_os = {
  'mac': {
    'src/third_party/prout': '/trunk/third_party/prout',
  },
}""" % { 'svn_base': self.svn_base })
    self._commit_svn(fs)

    fs = file_system(2, """
deps = {
  'src/other': '%(svn_base)strunk/other',
  # Load another DEPS and load a dependency from it. That's an example of
  # WebKit's chromium checkout flow. Verify it works out of order.
  'src/third_party/foo': From('src/file/other', 'foo/bar'),
  'src/file/other': File('%(svn_base)strunk/other/DEPS'),
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
""" % { 'svn_base': self.svn_base })
    fs['trunk/other/DEPS'] = """
deps = {
  'foo/bar': '/trunk/third_party/foo@1',
  # Only the requested deps should be processed.
  'invalid': '/does_not_exist',
}
"""
    # WebKit abuses this.
    fs['trunk/webkit/.gclient'] = """
solutions = [
  {
    'name': './',
    'url': None,
  },
]
"""
    fs['trunk/webkit/DEPS'] = """
deps = {
  'foo/bar': '%(svn_base)strunk/third_party/foo@1'
}

hooks = [
  {
    'pattern': '.*',
    'action': ['echo', 'foo'],
  },
]
""" % { 'svn_base': self.svn_base }
    self._commit_svn(fs)

  def populateGit(self):
    # Testing:
    # - dependency disapear
    # - dependency renamed
    # - versioned and unversioned reference
    # - relative and full reference
    # - deps_os
    # - var
    # - hooks
    # - From
    # TODO(maruel):
    # - File: File is hard to test here because it's SVN-only. It's
    #         implementation should probably be replaced to use urllib instead.
    # - $matching_files
    # - use_relative_paths
    self._commit_git('repo_3', {
      'origin': 'git/repo_3@1\n',
    })

    self._commit_git('repo_3', {
      'origin': 'git/repo_3@2\n',
    })

    self._commit_git('repo_1', {
      'DEPS': """
vars = {
  'DummyVariable': 'repo',
}
deps = {
  'src/repo2': '%(git_base)srepo_2',
  'src/repo2/repo3': '/' + Var('DummyVariable') + '_3@%(hash3)s',
}
deps_os = {
  'mac': {
    'src/repo4': '/repo_4',
  },
}""" % {
            'git_base': self.git_base,
            # See self.__init__() for the format. Grab's the hash of the first
            # commit in repo_2. Only keep the first 7 character because of:
            # TODO(maruel): http://crosbug.com/3591 We need to strip the hash..
            # duh.
            'hash3': self.git_hashes['repo_3'][1][0][:7]
        },
        'origin': 'git/repo_1@1\n',
    })

    self._commit_git('repo_2', {
      'origin': 'git/repo_2@1\n',
      'DEPS': """
deps = {
  'foo/bar': '/repo_3',
}
""",
    })

    self._commit_git('repo_2', {
      'origin': 'git/repo_2@2\n',
    })

    self._commit_git('repo_4', {
      'origin': 'git/repo_4@1\n',
    })

    self._commit_git('repo_4', {
      'origin': 'git/repo_4@2\n',
    })

    self._commit_git('repo_1', {
      'DEPS': """
deps = {
  'src/repo2': '%(git_base)srepo_2@%(hash)s',
  #'src/repo2/repo_renamed': '/repo_3',
  'src/repo2/repo_renamed': From('src/repo2', 'foo/bar'),
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
        'git_base': self.git_base,
        # See self.__init__() for the format. Grab's the hash of the first
        # commit in repo_2. Only keep the first 7 character because of:
        # TODO(maruel): http://crosbug.com/3591 We need to strip the hash.. duh.
        'hash': self.git_hashes['repo_2'][1][0][:7]
      },
      'origin': 'git/repo_1@2\n',
    })


class FakeReposTestBase(trial_dir.TestCase):
  """This is vaguely inspired by twisted."""
  # static FakeRepos instance. Lazy loaded.
  FAKE_REPOS = None
  # Override if necessary.
  FAKE_REPOS_CLASS = FakeRepos

  def setUp(self):
    super(FakeReposTestBase, self).setUp()
    if not FakeReposTestBase.FAKE_REPOS:
      # Lazy create the global instance.
      FakeReposTestBase.FAKE_REPOS = self.FAKE_REPOS_CLASS()
    # No need to call self.FAKE_REPOS.setUp(), it will be called by the child
    # class.
    # Do not define tearDown(), since super's version does the right thing and
    # FAKE_REPOS is kept across tests.

  @property
  def svn_base(self):
    """Shortcut."""
    return self.FAKE_REPOS.svn_base

  @property
  def git_base(self):
    """Shortcut."""
    return self.FAKE_REPOS.git_base

  def checkString(self, expected, result, msg=None):
    """Prints the diffs to ease debugging."""
    if expected != result:
      # Strip the begining
      while expected and result and expected[0] == result[0]:
        expected = expected[1:]
        result = result[1:]
      # The exception trace makes it hard to read so dump it too.
      if '\n' in result:
        print result
    self.assertEquals(expected, result, msg)

  def check(self, expected, results):
    """Checks stdout, stderr, returncode."""
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

  def mangle_svn_tree(self, *args):
    """Creates a 'virtual directory snapshot' to compare with the actual result
    on disk."""
    result = {}
    for item, new_root in args:
      old_root, rev = item.split('@', 1)
      tree = self.FAKE_REPOS.svn_revs[int(rev)]
      for k, v in tree.iteritems():
        if not k.startswith(old_root):
          continue
        item = k[len(old_root) + 1:]
        if item.startswith('.'):
          continue
        result[join(new_root, item).replace(os.sep, '/')] = v
    return result

  def mangle_git_tree(self, *args):
    """Creates a 'virtual directory snapshot' to compare with the actual result
    on disk."""
    result = {}
    for item, new_root in args:
      repo, rev = item.split('@', 1)
      tree = self.gittree(repo, rev)
      for k, v in tree.iteritems():
        result[join(new_root, k)] = v
    return result

  def githash(self, repo, rev):
    """Sort-hand: Returns the hash for a git 'revision'."""
    return self.FAKE_REPOS.git_hashes[repo][int(rev)][0]

  def gittree(self, repo, rev):
    """Sort-hand: returns the directory tree for a git 'revision'."""
    return self.FAKE_REPOS.git_hashes[repo][int(rev)][1]


def main(argv):
  fake = FakeRepos()
  print 'Using %s' % fake.root_dir
  try:
    fake.set_up_svn()
    fake.set_up_git()
    print('Fake setup, press enter to quit or Ctrl-C to keep the checkouts.')
    sys.stdin.readline()
  except KeyboardInterrupt:
    trial_dir.TrialDir.SHOULD_LEAK.leak = True
  return 0


if __name__ == '__main__':
  sys.exit(main(sys.argv))
