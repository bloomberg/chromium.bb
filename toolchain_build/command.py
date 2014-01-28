#!/usr/bin/python
# Copyright (c) 2012 The Native Client Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Class capturing a command invocation as data."""


# Done first to setup python module path.
import toolchain_env

import inspect
import hashlib
import os
import shutil
import sys

import file_tools
import log_tools
import repo_tools
import substituter


# MSYS tools do not always work with combinations of Windows and MSYS
# path conventions, e.g. '../foo\\bar' doesn't find '../foo/bar'.
# Since we convert all the directory names to MSYS conventions, we
# should not be using Windows separators with those directory names.
# As this is really an implementation detail of this module, we export
# 'command.path' to use in place of 'os.path', rather than having
# users of the module know which flavor to use.
import posixpath
path = posixpath


SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
NACL_DIR = os.path.dirname(SCRIPT_DIR)

COMMAND_CODE_FILES = [os.path.join(SCRIPT_DIR, f)
                      for f in ('command.py', 'once.py', 'substituter.py',
                                'pnacl_commands.py', 'toolchain_env.py',
                                'toolchain_main.py')]
COMMAND_CODE_FILES += [os.path.join(NACL_DIR, 'build', f)
                       for f in ('directory_storage.py', 'file_tools.py',
                                 'gsd_storage.py', 'hashing_tools.py',
                                 'local_storage_cache.py', 'log_tools.py',
                                 'platform_tools.py', 'repo_tools.py')]

def HashBuildSystemSources():
  """Read the build source files to use in hashes for Callbacks."""
  global FILE_CONTENTS_HASH
  h = hashlib.sha1()
  for filename in COMMAND_CODE_FILES:
    with open(filename) as f:
      h.update(f.read())
  FILE_CONTENTS_HASH = h.hexdigest()

HashBuildSystemSources()


def PlatformEnvironment(extra_paths):
  """Select the environment variables to run commands with.

  Args:
    extra_paths: Extra paths to add to the PATH variable.
  Returns:
    A dict to be passed as env to subprocess.
  """
  env = os.environ.copy()
  paths = []
  if sys.platform == 'win32':
    if Runnable.use_cygwin:
      # Use the hermetic cygwin.
      paths = [os.path.join(NACL_DIR, 'cygwin', 'bin')]
    else:
      # TODO(bradnelson): switch to something hermetic.
      mingw = os.environ.get('MINGW', r'c:\mingw')
      msys = os.path.join(mingw, 'msys', '1.0')
      if not os.path.exists(msys):
        msys = os.path.join(mingw, 'msys')
      # We need both msys (posix like build environment) and MinGW (windows
      # build of tools like gcc). We add <MINGW>/msys/[1.0/]bin to the path to
      # get sh.exe. We add <MINGW>/bin to allow direct invocation on MinGW
      # tools. We also add an msys style path (/mingw/bin) to get things like
      # gcc from inside msys.
      paths = [
          '/mingw/bin',
          os.path.join(mingw, 'bin'),
          os.path.join(msys, 'bin'),
      ]
  env['PATH'] = os.pathsep.join(
      paths + extra_paths + env.get('PATH', '').split(os.pathsep))
  return env


class Runnable(object):
  """An object representing a single command."""
  use_cygwin = False

  def __init__(self, func, *args, **kwargs):
    """Construct a runnable which will call 'func' with 'args' and 'kwargs'.

    Args:
      func: Function which will be called by Invoke
      args: Positional arguments to be passed to func
      kwargs: Keyword arguments to be passed to func

      RUNNABLES SHOULD ONLY BE IMPLEMENTED IN THIS FILE, because their
      string representation (which is used to calculate whether targets should
      be rebuilt) is based on this file's hash and does not attempt to capture
      the code or bound variables of the function itself (the one exception is
      once_test.py which injects its own callbacks to verify its expectations).

      When 'func' is called, its first argument will be a substitution object
      which it can use to substitute %-templates in its arguments.
    """
    self._func = func
    self._args = args or []
    self._kwargs = kwargs or {}

  def __str__(self):
    values = []

    sourcefile = inspect.getsourcefile(self._func)
    # Check that the code for the runnable is implemented in one of the known
    # source files of the build system (which are included in its hash). This
    # isn't a perfect test because it could still import code from an outside
    # module, so we should be sure to add any new build system files to the list
    found_match = (os.path.basename(sourcefile) in
                   [os.path.basename(f) for f in
                    COMMAND_CODE_FILES + ['once_test.py']])
    if not found_match:
      print 'Function', self._func.func_name, 'in', sourcefile
      raise Exception('Python Runnable objects must be implemented in one of' +
                      'the following files: ' + str(COMMAND_CODE_FILES))

    # Like repr(datum), but do something stable for dictionaries.
    # This only properly handles dictionaries that use simple types
    # as keys.
    def ReprForHash(datum):
      if isinstance(datum, dict):
        # The order of a dictionary's items is unpredictable.
        # Manually make a string in dict syntax, but sorted on keys.
        return ('{' +
                ', '.join(repr(key) + ': ' + ReprForHash(value)
                          for key, value in sorted(datum.iteritems(),
                                                   key=lambda t: t[0])) +
                '}')
      elif isinstance(datum, list):
        # A list is already ordered, but its items might be dictionaries.
        return ('[' +
                ', '.join(ReprForHash(value) for value in datum) +
                ']')
      else:
        return repr(datum)

    for v in self._args:
      values += [ReprForHash(v)]
    # The order of a dictionary's items is unpredictable.
    # Sort by key for hashing purposes.
    for k, v in sorted(self._kwargs.iteritems(), key=lambda t: t[0]):
      values += [repr(k), ReprForHash(v)]
    values += [FILE_CONTENTS_HASH]

    return '\n'.join(values)

  def Invoke(self, subst):
    return self._func(subst, *self._args, **self._kwargs)


def Command(command, stdout=None, **kwargs):
  """Return a Runnable which invokes 'command' with check_call.

  Args:
    command: List or string with a command suitable for check_call
    stdout (optional): File name to redirect command's stdout to
    kwargs: Keyword arguments suitable for check_call (or 'cwd' or 'path_dirs')

  The command will be %-substituted and paths will be assumed to be relative to
  the cwd given by Invoke. If kwargs contains 'cwd' it will be appended to the
  cwd given by Invoke and used as the cwd for the call. If kwargs contains
  'path_dirs', the directories therein will be added to the paths searched for
  the command. Any other kwargs will be passed to check_call.
  """
  def runcmd(subst, command, stdout, **kwargs):
    check_call_kwargs = kwargs.copy()
    command = command[:]

    cwd = subst.SubstituteAbsPaths(check_call_kwargs.get('cwd', '.'))
    subst.SetCwd(cwd)
    check_call_kwargs['cwd'] = cwd

    # Extract paths from kwargs and add to the command environment.
    path_dirs = []
    if 'path_dirs' in check_call_kwargs:
      path_dirs = [subst.Substitute(dirname) for dirname
                   in check_call_kwargs['path_dirs']]
      del check_call_kwargs['path_dirs']
    check_call_kwargs['env'] = PlatformEnvironment(path_dirs)

    if isinstance(command, str):
      command = subst.Substitute(command)
    else:
      command = [subst.Substitute(arg) for arg in command]
      paths = check_call_kwargs['env']['PATH'].split(os.pathsep)
      command[0] = file_tools.Which(command[0], paths=paths)

    if stdout is not None:
      stdout = subst.SubstituteAbsPaths(stdout)

    log_tools.CheckCall(command, stdout=stdout, **check_call_kwargs)

  return Runnable(runcmd, command, stdout, **kwargs)

def SkipForIncrementalCommand(command, **kwargs):
  """Return a command which has the skip_for_incremental property set on it.

  This will cause the command to be skipped for incremental builds, if the
  working directory is not empty.
  """
  cmd = Command(command, **kwargs)
  cmd.skip_for_incremental = True
  return cmd

def Mkdir(path, parents=False):
  """Convenience method for generating mkdir commands."""
  def mkdir(subst, path):
    path = subst.SubstituteAbsPaths(path)
    if parents:
      os.makedirs(path)
    else:
      os.mkdir(path)
  return Runnable(mkdir, path)


def Copy(src, dst):
  """Convenience method for generating cp commands."""
  def copy(subst, src, dst):
    shutil.copyfile(subst.SubstituteAbsPaths(src),
                    subst.SubstituteAbsPaths(dst))
  return Runnable(copy, src, dst)


def CopyTree(src, dst, exclude=[]):
  """Copy a directory tree, excluding a list of top-level entries."""
  def copyTree(subst, src, dst, exclude):
    src = subst.SubstituteAbsPaths(src)
    dst = subst.SubstituteAbsPaths(dst)
    def ignoreExcludes(dir, files):
      if dir == src:
        return exclude
      else:
        return []
    file_tools.RemoveDirectoryIfPresent(dst)
    shutil.copytree(src, dst, symlinks=True, ignore=ignoreExcludes)
  return Runnable(copyTree, src, dst, exclude)


def RemoveDirectory(path):
  """Convenience method for generating a command to remove a directory tree."""
  def remove(subst, path):
    file_tools.RemoveDirectoryIfPresent(subst.SubstituteAbsPaths(path))
  return Runnable(remove, path)


def Remove(path):
  """Convenience method for generating a command to remove a file."""
  def remove(subst, path):
    path = subst.SubstituteAbsPaths(path)
    if os.path.exists(path):
      os.remove(path)
  return Runnable(remove, path)


def Rename(src, dst):
  """Convenience method for generating a command to rename a file."""
  def rename(subst, src, dst):
    os.rename(subst.SubstituteAbsPaths(src), subst.SubstituteAbsPaths(dst))
  return Runnable(rename, src, dst)


def WriteData(data, dst):
  """Convenience method to write a file with fixed contents."""
  def writedata(subst, dst, data):
    with open(subst.SubstituteAbsPaths(dst), 'wb') as f:
      f.write(data)
  return Runnable(writedata, dst, data)


def SyncGitRepo(url, destination, revision, reclone=False, clean=False,
                pathspec=None):
  def sync(subst, url, dest, rev, reclone, clean, pathspec):
    repo_tools.SyncGitRepo(url, subst.SubstituteAbsPaths(dest), revision,
                           reclone, clean, pathspec)
  return Runnable(sync, url, destination, revision, reclone, clean, pathspec)


def CleanGitWorkingDir(directory, path):
  """Clean a path in a git checkout, if the checkout directory exists."""
  def clean(subst, directory, path):
    directory = subst.SubstituteAbsPaths(directory)
    if os.path.exists(directory) and len(os.listdir(directory)) > 0:
      repo_tools.CleanGitWorkingDir(directory, path)
  return Runnable(clean, directory, path)


def GenerateGitPatches(git_dir, info):
  """Generate patches from a Git repository.

  Args:
    git_dir: bare git repository directory to examine (.../.git)
    info: dictionary containing:
      'rev': commit that we build
      'upstream-name': basename of the upstream baseline release
        (i.e. what the release tarball would be called before ".tar")
      'upstream-base': commit corresponding to upstream-name
      'upstream-branch': tracking branch used for upstream merges

  This will produce between zero and two patch files (in %(output)s/):
    <upstream-name>-g<commit-abbrev>.patch: From 'upstream-base' to the common
      ancestor (merge base) of 'rev' and 'upstream-branch'.  Omitted if none.
    <upstream-name>[-g<commit-abbrev>]-nacl.patch: From the result of that
      (or from 'upstream-base' if none above) to 'rev'.
  """
  def generatePatches(subst, git_dir, info):
    git_dir_flag = '--git-dir=' + subst.SubstituteAbsPaths(git_dir)
    basename = info['upstream-name']

    def generatePatch(src_rev, dst_rev, suffix):
      src_prefix = '--src-prefix=' + basename + '/'
      dst_prefix = '--dst-prefix=' + basename + suffix + '/'
      patch_file = subst.SubstituteAbsPaths(
          path.join('%(output)s', basename + suffix + '.patch'))
      git_args = [git_dir_flag, 'diff',
                  '--patch-with-stat', '--ignore-space-at-eol', '--full-index',
                  '--no-ext-diff', '--no-color', '--no-renames',
                  '--no-textconv', '--text', src_prefix, dst_prefix,
                  src_rev, dst_rev]
      log_tools.CheckCall(repo_tools.GitCmd() + git_args, stdout=patch_file)

    def revParse(args):
      output = repo_tools.CheckGitOutput([git_dir_flag] + args)
      lines = output.splitlines()
      if len(lines) != 1:
        raise Exception('"git %s" did not yield a single commit' %
                        ' '.join(args))
      return lines[0]

    rev = revParse(['rev-parse', info['rev']])
    upstream_base = revParse(['rev-parse', info['upstream-base']])
    upstream_branch = revParse(['rev-parse',
                                'refs/remotes/origin/' +
                                info['upstream-branch']])
    upstream_snapshot = revParse(['merge-base', rev, upstream_branch])

    if rev == upstream_base:
      # We're building a stock upstream release.  Nothing to do!
      return

    if upstream_snapshot == upstream_base:
      # We've forked directly from the upstream baseline release.
      suffix = ''
    else:
      # We're using an upstream baseline snapshot past the baseline
      # release, so generate a snapshot patch.  The leading seven
      # hex digits of the commit ID is what Git usually produces
      # for --abbrev-commit behavior, 'git describe', etc.
      suffix = '-g' + upstream_snapshot[:7]
      generatePatch(upstream_base, upstream_snapshot, suffix)

    if rev != upstream_snapshot:
      # We're using local changes, so generate a patch of those.
      generatePatch(upstream_snapshot, rev, suffix + '-nacl')
  return Runnable(generatePatches, git_dir, info)
