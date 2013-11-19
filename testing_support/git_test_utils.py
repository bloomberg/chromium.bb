# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import atexit
import collections
import copy
import datetime
import hashlib
import os
import shutil
import subprocess
import tempfile
import unittest


def git_hash_data(data, typ='blob'):
  """Calculate the git-style SHA1 for some data.

  Only supports 'blob' type data at the moment.
  """
  assert typ == 'blob', 'Only support blobs for now'
  return hashlib.sha1('blob %s\0%s' % (len(data), data)).hexdigest()


class OrderedSet(collections.MutableSet):
  # from http://code.activestate.com/recipes/576694/
  def __init__(self, iterable=None):
    self.end = end = []
    end += [None, end, end]         # sentinel node for doubly linked list
    self.data = {}                  # key --> [key, prev, next]
    if iterable is not None:
      self |= iterable

  def __contains__(self, key):
    return key in self.data

  def __eq__(self, other):
    if isinstance(other, OrderedSet):
      return len(self) == len(other) and list(self) == list(other)
    return set(self) == set(other)

  def __ne__(self, other):
    if isinstance(other, OrderedSet):
      return len(self) != len(other) or list(self) != list(other)
    return set(self) != set(other)

  def __len__(self):
    return len(self.data)

  def __iter__(self):
    end = self.end
    curr = end[2]
    while curr is not end:
      yield curr[0]
      curr = curr[2]

  def __repr__(self):
    if not self:
      return '%s()' % (self.__class__.__name__,)
    return '%s(%r)' % (self.__class__.__name__, list(self))

  def __reversed__(self):
    end = self.end
    curr = end[1]
    while curr is not end:
      yield curr[0]
      curr = curr[1]

  def add(self, key):
    if key not in self.data:
      end = self.end
      curr = end[1]
      curr[2] = end[1] = self.data[key] = [key, curr, end]

  def difference_update(self, *others):
    for other in others:
      for i in other:
        self.discard(i)

  def discard(self, key):
    if key in self.data:
      key, prev, nxt = self.data.pop(key)
      prev[2] = nxt
      nxt[1] = prev

  def pop(self, last=True):  # pylint: disable=W0221
    if not self:
      raise KeyError('set is empty')
    key = self.end[1][0] if last else self.end[2][0]
    self.discard(key)
    return key


class GitRepoSchema(object):
  """A declarative git testing repo.

  Pass a schema to __init__ in the form of:
     A B C D
       B E D

  This is the repo

     A - B - C - D
           \ E /

  Whitespace doesn't matter. Each line is a declaration of which commits come
  before which other commits.

  Every commit gets a tag 'tag_%(commit)s'
  Every unique terminal commit gets a branch 'branch_%(commit)s'
  Last commit in First line is the branch 'master'
  Root commits get a ref 'root_%(commit)s'

  Timestamps are in topo order, earlier commits (as indicated by their presence
  in the schema) get earlier timestamps. Stamps start at the Unix Epoch, and
  increment by 1 day each.
  """
  COMMIT = collections.namedtuple('COMMIT', 'name parents is_branch is_root')

  def __init__(self, repo_schema='',
               content_fn=lambda v: {v: {'data': v}}):
    """Builds a new GitRepoSchema.

    Args:
      repo_schema (str) - Initial schema for this repo. See class docstring for
        info on the schema format.
      content_fn ((commit_name) -> commit_data) - A function which will be
        lazily called to obtain data for each commit. The results of this
        function are cached (i.e. it will never be called twice for the same
        commit_name). See the docstring on the GitRepo class for the format of
        the data returned by this function.
    """
    self.master = None
    self.par_map = {}
    self.data_cache = {}
    self.content_fn = content_fn
    self.add_commits(repo_schema)

  def walk(self):
    """(Generator) Walks the repo schema from roots to tips.

    Generates GitRepoSchema.COMMIT objects for each commit.

    Throws an AssertionError if it detects a cycle.
    """
    is_root = True
    par_map = copy.deepcopy(self.par_map)
    while par_map:
      empty_keys = set(k for k, v in par_map.iteritems() if not v)
      assert empty_keys, 'Cycle detected! %s' % par_map

      for k in sorted(empty_keys):
        yield self.COMMIT(k, self.par_map[k],
                          not any(k in v for v in self.par_map.itervalues()),
                          is_root)
        del par_map[k]
      for v in par_map.itervalues():
        v.difference_update(empty_keys)
      is_root = False

  def add_commits(self, schema):
    """Adds more commits from a schema into the existing Schema.

    Args:
      schema (str) - See class docstring for info on schema format.

    Throws an AssertionError if it detects a cycle.
    """
    for commits in (l.split() for l in schema.splitlines() if l.strip()):
      parent = None
      for commit in commits:
        if commit not in self.par_map:
          self.par_map[commit] = OrderedSet()
        if parent is not None:
          self.par_map[commit].add(parent)
        parent = commit
      if parent and not self.master:
        self.master = parent
    for _ in self.walk():  # This will throw if there are any cycles.
      pass

  def reify(self):
    """Returns a real GitRepo for this GitRepoSchema"""
    return GitRepo(self)

  def data_for(self, commit):
    """Obtains the data for |commit|.

    See the docstring on the GitRepo class for the format of the returned data.

    Caches the result on this GitRepoSchema instance.
    """
    if commit not in self.data_cache:
      self.data_cache[commit] = self.content_fn(commit)
    return self.data_cache[commit]


class GitRepo(object):
  """Creates a real git repo for a GitRepoSchema.

  Obtains schema and content information from the GitRepoSchema.

  The format for the commit data supplied by GitRepoSchema.data_for is:
    {
      SPECIAL_KEY: special_value,
      ...
      "path/to/some/file": { 'data': "some data content for this file",
                              'mode': 0755 },
      ...
    }

  The SPECIAL_KEYs are the following attribues of the GitRepo class:
    * AUTHOR_NAME
    * AUTHOR_EMAIL
    * AUTHOR_DATE - must be a datetime.datetime instance
    * COMMITTER_NAME
    * COMMITTER_EMAIL
    * COMMITTER_DATE - must be a datetime.datetime instance

  For file content, if 'data' is None, then this commit will `git rm` that file.
  """
  BASE_TEMP_DIR = tempfile.mkdtemp(suffix='base', prefix='git_repo')
  atexit.register(shutil.rmtree, BASE_TEMP_DIR)

  # Singleton objects to specify specific data in a commit dictionary.
  AUTHOR_NAME = object()
  AUTHOR_EMAIL = object()
  AUTHOR_DATE = object()
  COMMITTER_NAME = object()
  COMMITTER_EMAIL = object()
  COMMITTER_DATE = object()

  DEFAULT_AUTHOR_NAME = 'Author McAuthorly'
  DEFAULT_AUTHOR_EMAIL = 'author@example.com'
  DEFAULT_COMMITTER_NAME = 'Charles Committish'
  DEFAULT_COMMITTER_EMAIL = 'commitish@example.com'

  COMMAND_OUTPUT = collections.namedtuple('COMMAND_OUTPUT', 'retcode stdout')

  def __init__(self, schema):
    """Makes new GitRepo.

    Automatically creates a temp folder under GitRepo.BASE_TEMP_DIR. It's
    recommended that you clean this repo up by calling nuke() on it, but if not,
    GitRepo will automatically clean up all allocated repos at the exit of the
    program (assuming a normal exit like with sys.exit)

    Args:
      schema - An instance of GitRepoSchema
    """
    self.repo_path = tempfile.mkdtemp(dir=self.BASE_TEMP_DIR)
    self.commit_map = {}
    self._date = datetime.datetime(1970, 1, 1)

    self.git('init')
    for commit in schema.walk():
      self._add_schema_commit(commit, schema.data_for(commit.name))
    if schema.master:
      self.git('update-ref', 'master', self[schema.master])

  def __getitem__(self, commit_name):
    """Gets the hash of a commit by its schema name.

    >>> r = GitRepo(GitRepoSchema('A B C'))
    >>> r['B']
    '7381febe1da03b09da47f009963ab7998a974935'
    """
    return self.commit_map[commit_name]

  def _add_schema_commit(self, commit, data):
    data = data or {}

    if commit.parents:
      parents = list(commit.parents)
      self.git('checkout', '--detach', '-q', self[parents[0]])
      if len(parents) > 1:
        self.git('merge', '--no-commit', '-q', *[self[x] for x in parents[1:]])
    else:
      self.git('checkout', '--orphan', 'root_%s' % commit.name)
      self.git('rm', '-rf', '.')

    env = {}
    for prefix in ('AUTHOR', 'COMMITTER'):
      for suffix in ('NAME', 'EMAIL', 'DATE'):
        singleton = '%s_%s' % (prefix, suffix)
        key = getattr(self, singleton)
        if key in data:
          val = data[key]
        else:
          if suffix == 'DATE':
            val = self._date
            self._date += datetime.timedelta(days=1)
          else:
            val = getattr(self, 'DEFAULT_%s' % singleton)
        env['GIT_%s' % singleton] = str(val)

    for fname, file_data in data.iteritems():
      deleted = False
      if 'data' in file_data:
        data = file_data.get('data')
        if data is None:
          deleted = True
          self.git('rm', fname)
        else:
          path = os.path.join(self.repo_path, fname)
          pardir = os.path.dirname(path)
          if not os.path.exists(pardir):
            os.makedirs(pardir)
          with open(path, 'wb') as f:
            f.write(data)

      mode = file_data.get('mode')
      if mode and not deleted:
        os.chmod(path, mode)

      self.git('add', fname)

    rslt = self.git('commit', '--allow-empty', '-m', commit.name, env=env)
    assert rslt.retcode == 0, 'Failed to commit %s' % str(commit)
    self.commit_map[commit.name] = self.git('rev-parse', 'HEAD').stdout.strip()
    self.git('tag', 'tag_%s' % commit.name, self[commit.name])
    if commit.is_branch:
      self.git('update-ref', 'branch_%s' % commit.name, self[commit.name])

  def git(self, *args, **kwargs):
    """Runs a git command specified by |args| in this repo."""
    assert self.repo_path is not None
    try:
      with open(os.devnull, 'wb') as devnull:
        output = subprocess.check_output(
          ('git',) + args, cwd=self.repo_path, stderr=devnull, **kwargs)
      return self.COMMAND_OUTPUT(0, output)
    except subprocess.CalledProcessError as e:
      return self.COMMAND_OUTPUT(e.returncode, e.output)

  def nuke(self):
    """Obliterates the git repo on disk.

    Causes this GitRepo to be unusable.
    """
    shutil.rmtree(self.repo_path)
    self.repo_path = None

  def run(self, fn, *args, **kwargs):
    """Run a python function with the given args and kwargs with the cwd set to
    the git repo."""
    assert self.repo_path is not None
    curdir = os.getcwd()
    try:
      os.chdir(self.repo_path)
      return fn(*args, **kwargs)
    finally:
      os.chdir(curdir)


class GitRepoSchemaTestBase(unittest.TestCase):
  """A TestCase with a built-in GitRepoSchema.

  Expects a class variable REPO to be a GitRepoSchema string in the form
  described by that class.

  You may also set class variables in the form COMMIT_%(commit_name)s, which
  provide the content for the given commit_name commits.

  You probably will end up using either GitRepoReadOnlyTestBase or
  GitRepoReadWriteTestBase for real tests.
  """
  REPO = None

  @classmethod
  def getRepoContent(cls, commit):
    return getattr(cls, 'COMMIT_%s' % commit, None)

  @classmethod
  def setUpClass(cls):
    super(GitRepoSchemaTestBase, cls).setUpClass()
    assert cls.REPO is not None
    cls.r_schema = GitRepoSchema(cls.REPO, cls.getRepoContent)


class GitRepoReadOnlyTestBase(GitRepoSchemaTestBase):
  """Injects a GitRepo object given the schema and content from
  GitRepoSchemaTestBase into TestCase classes which subclass this.

  This GitRepo will appear as self.repo, and will be deleted and recreated once
  for the duration of all the tests in the subclass.
  """
  REPO = None

  @classmethod
  def setUpClass(cls):
    super(GitRepoReadOnlyTestBase, cls).setUpClass()
    assert cls.REPO is not None
    cls.repo = cls.r_schema.reify()

  @classmethod
  def tearDownClass(cls):
    cls.repo.nuke()
    super(GitRepoReadOnlyTestBase, cls).tearDownClass()


class GitRepoReadWriteTestBase(GitRepoSchemaTestBase):
  """Injects a GitRepo object given the schema and content from
  GitRepoSchemaTestBase into TestCase classes which subclass this.

  This GitRepo will appear as self.repo, and will be deleted and recreated for
  each test function in the subclass.
  """
  REPO = None

  def setUp(self):
    super(GitRepoReadWriteTestBase, self).setUp()
    self.repo = self.r_schema.reify()

  def tearDown(self):
    self.repo.nuke()
    super(GitRepoReadWriteTestBase, self).tearDown()
