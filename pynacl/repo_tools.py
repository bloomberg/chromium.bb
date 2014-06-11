#!/usr/bin/python
# Copyright (c) 2013 The Native Client Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import logging
import os
import subprocess
import sys
import urlparse

import file_tools
import log_tools
import platform

class InvalidRepoException(Exception):
  def __init__(self, expected_repo, msg, *args):
    Exception.__init__(self, msg % args)
    self.expected_repo = expected_repo


def GitCmd():
  """Return the git command to execute for the host platform."""
  if platform.IsWindows():
    # On windows, we want to use the depot_tools version of git, which has
    # git.bat as an entry point. When running through the msys command
    # prompt, subprocess does not handle batch files. Explicitly invoking
    # cmd.exe to be sure we run the correct git in this case.
    return ['cmd.exe', '/c', 'git.bat']
  else:
    return ['git']


def CheckGitOutput(args):
  """Run a git subcommand and capture its stdout a la subprocess.check_output.
  Args:
    args: list of arguments to 'git'
  """
  return log_tools.CheckOutput(GitCmd() + args)


def SvnCmd():
  """Return the svn command to execute for the host platform."""
  if platform.IsWindows():
    return ['cmd.exe', '/c', 'svn.bat']
  else:
    return ['svn']


def ValidateGitRepo(url, directory, clobber_mismatch=False):
  """Validates a git repository tracks a particular URL.

  Given a git directory, this function will validate if the git directory
  actually tracks an expected URL. If the directory does not exist nothing
  will be done.

  Args:
  url: URL to look for.
  directory: Directory to look for.
  clobber_mismatch: If True, will delete invalid directories instead of raising
                    an exception.
  """
  git_dir = os.path.join(directory, '.git')
  if os.path.exists(git_dir):
    try:
      if IsURLInRemoteRepoList(url, directory, include_fetch=True,
                               include_push=False):
        return

      logging.warn('Local git repo (%s) does not track url (%s)',
                   directory, url)
    except:
      logging.error('Invalid git repo: %s', directory)

    if not clobber_mismatch:
      raise InvalidRepoException(url, 'Invalid local git repo: %s', directory)
    else:
      logging.debug('Clobbering invalid git repo %s' % directory)
      file_tools.RemoveDirectoryIfPresent(directory)
  elif os.path.exists(directory) and len(os.listdir(directory)) != 0:
    if not clobber_mismatch:
      raise InvalidRepoException(url,
                                 'Invalid non-empty repository destination %s',
                                 directory)
    else:
      logging.debug('Clobbering intended repository destination: %s', directory)
      file_tools.RemoveDirectoryIfPresent(directory)


def SyncGitRepo(url, destination, revision, reclone=False, clean=False,
                pathspec=None, git_cache=None, push_url=None):
  """Sync an individual git repo.

  Args:
  url: URL to sync
  destination: Directory to check out into.
  revision: Pinned revision to check out. If None, do not check out a
            pinned revision.
  reclone: If True, delete the destination directory and re-clone the repo.
  clean: If True, discard local changes and untracked files.
         Otherwise the checkout will fail if there are uncommitted changes.
  pathspec: If not None, add the path to the git checkout command, which
            causes it to just update the working tree without switching
            branches.
  git_cache: If set, assumes URL has been populated within the git cache
             directory specified and sets the fetch URL to be from the
             git_cache.
  """
  if reclone:
    logging.debug('Clobbering source directory %s' % destination)
    file_tools.RemoveDirectoryIfPresent(destination)

  if git_cache:
    fetch_url = GetGitCacheURL(git_cache, url)
  else:
    fetch_url = url

  # If the destination is a git repository, validate the tracked origin.
  git_dir = os.path.join(destination, '.git')
  if os.path.exists(git_dir):
    if not IsURLInRemoteRepoList(fetch_url, destination, include_fetch=True,
                                 include_push=False):
      # If the original URL is being tracked instead of the fetch URL, we
      # can safely redirect it to the fetch URL instead.
      if (fetch_url != url and IsURLInRemoteRepoList(url, destination,
                                                     include_fetch=True,
                                                     include_push=False)):
        GitSetRemoteRepo(fetch_url, destination, push_url=push_url)
      else:
        logging.error('Git Repo (%s) does not track URL: %s',
                      destination, fetch_url)
        raise InvalidRepoException(fetch_url, 'Could not sync git repo: %s',
                                   destination)

  git = GitCmd()
  if not os.path.exists(git_dir):
    logging.info('Cloning %s...' % url)
    clone_args = ['clone', '-n']
    if git_cache:
      clone_args.append('-s')

    file_tools.MakeDirectoryIfAbsent(destination)
    log_tools.CheckCall(git + clone_args + [fetch_url, '.'], cwd=destination)

    if fetch_url != url:
      GitSetRemoteRepo(fetch_url, destination, push_url=push_url)

  elif clean:
    log_tools.CheckCall(git + ['clean', '-dffx'], cwd=destination)
    log_tools.CheckCall(git + ['reset', '--hard', 'HEAD'], cwd=destination)

  if revision is not None:
    logging.info('Checking out pinned revision...')
    log_tools.CheckCall(git + ['fetch', '--all'], cwd=destination)
    checkout_flags = ['-f'] if clean else []
    path = [pathspec] if pathspec else []
    log_tools.CheckCall(
        git + ['checkout'] + checkout_flags + [revision] + path,
        cwd=destination)


def CleanGitWorkingDir(directory, path):
  """Clean a particular path of an existing git checkout.

     Args:
     directory: Directory where the git repo is currently checked out
     path: path to clean, relative to the repo directory
  """
  log_tools.CheckCall(GitCmd() + ['clean', '-f', path], cwd=directory)


def PopulateGitCache(cache_dir, url_list):
  """Fetches a git repo that combines a list of git repos.

  This is an interface to the "git cache" command found within depot_tools.
  You can populate a cache directory then obtain the local cache url using
  GetGitCacheURL(). It is best to sync with the shared option so that the
  cloned repository shares the same git objects.

  Args:
    cache_dir: Local directory where git cache will be populated.
    url_list: List of URLs which cache_dir should be populated with.
  """
  if url_list:
    file_tools.MakeDirectoryIfAbsent(cache_dir)
    git = GitCmd()
    for url in url_list:
      log_tools.CheckCall(git + ['cache', 'populate', '-c', '.', url],
                          cwd=cache_dir)


def GetGitCacheURL(cache_dir, url):
  """Converts a regular git URL to a git cache URL within a cache directory.

  Args:
    url: original Git URL that is already populated within the cache directory.
    cache_dir: Git cache directory that has already populated the URL.
  """
  # Make sure we are using absolute paths or else cache exists return relative.
  cache_dir = os.path.abspath(cache_dir)

  # For CygWin, we must first convert the cache_dir name to a non-cygwin path.
  if platform.IsWindows() and cache_dir.startswith('/cygdrive/'):
    drive, file_path = cache_dir[len('/cygdrive/'):].split('/', 1)
    cache_dir = drive.upper() + ':\\' + file_path.replace('/', '\\')

  return log_tools.CheckOutput(GitCmd() + ['cache', 'exists', '-c', cache_dir,
                                           url]).strip()


def GitRevInfo(directory):
  """Get the git revision information of a git checkout.

  Args:
    directory: Existing git working directory.
"""
  url = log_tools.CheckOutput(GitCmd() + ['ls-remote', '--get-url', 'origin'],
                              cwd=directory)
  rev = log_tools.CheckOutput(GitCmd() + ['rev-parse', 'HEAD'],
                              cwd=directory)
  return url.strip(), rev.strip()


def SvnRevInfo(directory):
  """Get the SVN revision information of an existing svn/gclient checkout.

  Args:
     directory: Directory where the svn repo is currently checked out
  """
  info = log_tools.CheckOutput(SvnCmd() + ['info'], cwd=directory)
  url = ''
  rev = ''
  for line in info.splitlines():
    pieces = line.split(':', 1)
    if len(pieces) != 2:
      continue
    if pieces[0] == 'URL':
      url = pieces[1].strip()
    elif pieces[0] == 'Revision':
      rev = pieces[1].strip()
  if not url or not rev:
    raise RuntimeError('Missing svn info url: %s and rev: %s' % (url, rev))
  return url, rev


def GetAuthenticatedGitURL(url):
  """Returns the authenticated version of a git URL.

  In chromium, there is a special URL that is the "authenticated" version. The
  URLs are identical but the authenticated one has special privileges.
  """
  urlsplit = urlparse.urlsplit(url)
  if urlsplit.scheme in ('https', 'http'):
    urldict = urlsplit._asdict()
    urldict['scheme'] = 'https'
    urldict['path'] = '/a' + urlsplit.path
    urlsplit = urlparse.SplitResult(**urldict)

  return urlsplit.geturl()


def GitRemoteRepoList(directory, include_fetch=True, include_push=True):
  """Returns a list of remote git repos associated with a directory.

  Args:
      directory: Existing git working directory.
  Returns:
      List of (repo_name, repo_url) for tracked remote repos.
  """
  remote_repos = log_tools.CheckOutput(GitCmd() + ['remote', '-v'],
                                       cwd=directory)

  repo_set = set()
  for remote_repo_line in remote_repos.splitlines():
    repo_name, repo_url, repo_type = remote_repo_line.split()
    if include_fetch and repo_type == '(fetch)':
      repo_set.add((repo_name, repo_url))
    elif include_push and repo_type == '(push)':
      repo_set.add((repo_name, repo_url))

  return sorted(repo_set)


def GitSetRemoteRepo(url, directory, push_url=None, repo_name='origin'):
  """Sets the remotely tracked URL for a git repository.

  Args:
      url: Remote git URL to set.
      directory: Local git repository to set tracked repo for.
      push_url: If specified, uses a different URL for pushing.
      repo_name: set the URL for a particular remote repo name.
  """
  git = GitCmd()
  try:
    log_tools.CheckCall(git + ['remote', 'set-url', repo_name, url],
                        cwd=directory)
  except subprocess.CalledProcessError:
    # If setting the URL failed, repo_name may be new. Try adding the URL.
    log_tools.CheckCall(git + ['remote', 'add', repo_name, url],
                        cwd=directory)

  if push_url:
    log_tools.CheckCall(git + ['remote', 'set-url', '--push',
                               repo_name, push_url],
                        cwd=directory)


def IsURLInRemoteRepoList(url, directory, include_fetch=True, include_push=True,
                          try_authenticated_url=True):
  """Returns whether or not a url is a remote repo in a local git directory.

  Args:
      url: URL to look for in remote repo list.
      directory: Existing git working directory.
  """
  if try_authenticated_url:
    valid_urls = (url, GetAuthenticatedGitURL(url))
  else:
    valid_urls = (url,)

  remote_repo_list = GitRemoteRepoList(directory,
                                       include_fetch=include_fetch,
                                       include_push=include_push)
  return len([repo_name for
              repo_name, repo_url in remote_repo_list
              if repo_url in valid_urls]) > 0
